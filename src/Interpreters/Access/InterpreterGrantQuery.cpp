#include <Interpreters/InterpreterFactory.h>
#include <Interpreters/Access/InterpreterGrantQuery.h>
#include <Parsers/Access/ASTGrantQuery.h>
#include <Parsers/Access/ASTRolesOrUsersSet.h>
#include <Access/AccessControl.h>
#include <Access/ContextAccess.h>
#include <Access/Role.h>
#include <Access/RolesOrUsersSet.h>
#include <Access/User.h>
#include <Interpreters/Context.h>
#include <Interpreters/removeOnClusterClauseIfNeeded.h>
#include <Interpreters/QueryLog.h>
#include <Interpreters/executeDDLQueryOnCluster.h>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/set_algorithm.hpp>

namespace DB
{
namespace ErrorCodes
{
    extern const int BAD_ARGUMENTS;
    extern const int LOGICAL_ERROR;
}

namespace
{
    /// Extracts access rights elements which are going to be granted or revoked from a query.
    void collectAccessRightsElementsToGrantOrRevoke(
        const ASTGrantQuery & query,
        AccessRightsElements & elements_to_grant,
        AccessRightsElements & elements_to_revoke)
    {
        elements_to_grant.clear();
        elements_to_revoke.clear();

        if (query.is_revoke)
        {
            /// REVOKE
            elements_to_revoke = query.access_rights_elements;
        }
        else if (query.replace_access)
        {
            /// GRANT WITH REPLACE OPTION
            elements_to_grant = query.access_rights_elements;
            elements_to_revoke.emplace_back(AccessType::ALL);
        }
        else
        {
            /// GRANT
            elements_to_grant = query.access_rights_elements;
        }
    }

    /// Extracts roles which are going to be granted or revoked from a query.
    void collectRolesToGrantOrRevoke(
        const AccessControl & access_control,
        const ASTGrantQuery & query,
        std::vector<UUID> & roles_to_grant,
        RolesOrUsersSet & roles_to_revoke)
    {
        roles_to_grant.clear();
        roles_to_revoke.clear();

        RolesOrUsersSet roles_to_grant_or_revoke;
        if (query.roles)
            roles_to_grant_or_revoke = RolesOrUsersSet{*query.roles, access_control};

        if (query.is_revoke)
        {
            /// REVOKE
            roles_to_revoke = std::move(roles_to_grant_or_revoke);
        }
        else if (query.replace_granted_roles)
        {
            /// GRANT WITH REPLACE OPTION
            roles_to_grant = roles_to_grant_or_revoke.getMatchingIDs(access_control);
            roles_to_revoke = RolesOrUsersSet::AllTag{};
        }
        else
        {
            /// GRANT
            roles_to_grant = roles_to_grant_or_revoke.getMatchingIDs(access_control);
        }
    }

    /// Extracts roles which are going to be granted or revoked from a query.
    void collectRolesToGrantOrRevoke(
        const ASTGrantQuery & query,
        std::vector<UUID> & roles_to_grant,
        RolesOrUsersSet & roles_to_revoke)
    {
        roles_to_grant.clear();
        roles_to_revoke.clear();

        RolesOrUsersSet roles_to_grant_or_revoke;
        if (query.roles)
            roles_to_grant_or_revoke = RolesOrUsersSet{*query.roles};

        if (query.is_revoke)
        {
            /// REVOKE
            roles_to_revoke = std::move(roles_to_grant_or_revoke);
        }
        else if (query.replace_granted_roles)
        {
            /// GRANT WITH REPLACE OPTION
            roles_to_grant = roles_to_grant_or_revoke.getMatchingIDs();
            roles_to_revoke = RolesOrUsersSet::AllTag{};
        }
        else
        {
            /// GRANT
            roles_to_grant = roles_to_grant_or_revoke.getMatchingIDs();
        }
    }

    /// Checks if the current user has enough access rights granted with grant option to grant or revoke specified access rights.
    void checkGrantOption(
        const AccessControl & access_control,
        const ContextAccessWrapper & current_user_access,
        const std::vector<UUID> & grantees_from_query,
        bool & need_check_grantees_are_allowed,
        const AccessRightsElements & elements_to_grant,
        AccessRightsElements & elements_to_revoke)
    {
        /// Check access rights which are going to be granted.
        /// To execute the command GRANT the current user needs to have the access granted with GRANT OPTION.
        current_user_access.checkGrantOption(elements_to_grant);

        if (current_user_access.hasGrantOption(elements_to_revoke))
        {
            /// Simple case: the current user has the grant option for all the access rights specified for REVOKE.
            return;
        }

        /// Special case for the command REVOKE: it's possible that the current user doesn't have
        /// the access granted with GRANT OPTION but it's still ok because the roles or users
        /// from whom the access rights will be revoked don't have the specified access granted either.
        ///
        /// For example, to execute
        /// GRANT ALL ON mydb.* TO role1
        /// REVOKE ALL ON *.* FROM role1
        /// the current user needs to have the grants only on the 'mydb' database.
        AccessRights all_granted_access;
        for (const auto & id : grantees_from_query)
        {
            auto entity = access_control.tryRead(id);
            if (auto role = typeid_cast<RolePtr>(entity))
            {
                if (need_check_grantees_are_allowed)
                    current_user_access.checkGranteeIsAllowed(id, *role);
                all_granted_access.makeUnion(role->access);
            }
            else if (auto user = typeid_cast<UserPtr>(entity))
            {
                if (need_check_grantees_are_allowed)
                    current_user_access.checkGranteeIsAllowed(id, *user);
                all_granted_access.makeUnion(user->access);
            }
        }

        need_check_grantees_are_allowed = false; /// already checked

        if (!elements_to_revoke.empty() && elements_to_revoke[0].is_partial_revoke)
            std::for_each(elements_to_revoke.begin(), elements_to_revoke.end(), [&](AccessRightsElement & element) { element.is_partial_revoke = false; });
        AccessRights access_to_revoke;
        access_to_revoke.grant(elements_to_revoke);
        access_to_revoke.makeIntersection(all_granted_access);

        /// Build more accurate list of elements to revoke, now we use an intersection of the initial list of elements to revoke
        /// and all the granted access rights to these grantees.
        bool grant_option = !elements_to_revoke.empty() && elements_to_revoke[0].grant_option;
        elements_to_revoke.clear();
        for (auto & element_to_revoke : access_to_revoke.getElements())
        {
            if (!element_to_revoke.is_partial_revoke && (element_to_revoke.grant_option || !grant_option))
                elements_to_revoke.emplace_back(std::move(element_to_revoke));
        }

        /// Additional check for REVOKE
        ///
        /// If user1 has the rights
        /// GRANT SELECT ON *.* TO user1;
        /// REVOKE SELECT ON system.* FROM user1;
        /// REVOKE SELECT ON mydb.* FROM user1;
        ///
        /// And user2 has the rights
        /// GRANT SELECT ON *.* TO user2;
        /// REVOKE SELECT ON system.* FROM user2;
        ///
        /// the query `REVOKE SELECT ON *.* FROM user1` executed by user2 should succeed.
        if (current_user_access.getAccessRightsWithImplicit()->containsWithGrantOption(access_to_revoke))
            return;

        /// Technically, this check always fails if `containsWithGrantOption` returns `false`. But we still call it to get a nice exception message.
        current_user_access.checkGrantOption(elements_to_revoke);
    }

    /// Checks if the current user has enough roles granted with admin option to grant or revoke specified roles.
    void checkAdminOption(
        const AccessControl & access_control,
        const ContextAccessWrapper & current_user_access,
        const std::vector<UUID> & grantees_from_query,
        bool & need_check_grantees_are_allowed,
        const std::vector<UUID> & roles_to_grant,
        RolesOrUsersSet & roles_to_revoke,
        bool admin_option)
    {
        /// Check roles which are going to be granted.
        /// To execute the command GRANT the current user needs to have the roles granted with ADMIN OPTION.
        current_user_access.checkAdminOption(roles_to_grant);

        /// Check roles which are going to be revoked.
        std::vector<UUID> roles_to_revoke_ids;
        if (!roles_to_revoke.all)
        {
            roles_to_revoke_ids = roles_to_revoke.getMatchingIDs();
            if (current_user_access.hasAdminOption(roles_to_revoke_ids))
            {
                /// Simple case: the current user has the admin option for all the roles specified for REVOKE.
                return;
            }
        }

        /// Special case for the command REVOKE: it's possible that the current user doesn't have the admin option
        /// for some of the specified roles but it's still ok because the roles or users from whom the roles will be
        /// revoked from don't have the specified roles granted either.
        ///
        /// For example, to execute
        /// GRANT role2 TO role1
        /// REVOKE ALL FROM role1
        /// the current user needs to have only 'role2' to be granted with admin option (not all the roles).
        GrantedRoles all_granted_roles;
        for (const auto & id : grantees_from_query)
        {
            auto entity = access_control.tryRead(id);
            if (auto role = typeid_cast<RolePtr>(entity))
            {
                if (need_check_grantees_are_allowed)
                    current_user_access.checkGranteeIsAllowed(id, *role);
                all_granted_roles.makeUnion(role->granted_roles);
            }
            else if (auto user = typeid_cast<UserPtr>(entity))
            {
                if (need_check_grantees_are_allowed)
                    current_user_access.checkGranteeIsAllowed(id, *user);
                all_granted_roles.makeUnion(user->granted_roles);
            }
        }

        need_check_grantees_are_allowed = false; /// already checked

        const auto & all_granted_roles_set = admin_option ? all_granted_roles.getGrantedWithAdminOption() : all_granted_roles.getGranted();
        if (roles_to_revoke.all)
            boost::range::set_difference(all_granted_roles_set, roles_to_revoke.except_ids, std::back_inserter(roles_to_revoke_ids));
        else
            std::erase_if(roles_to_revoke_ids, [&](const UUID & id) { return !all_granted_roles_set.count(id); });

        roles_to_revoke = roles_to_revoke_ids;
        current_user_access.checkAdminOption(roles_to_revoke_ids);
    }

    /// Returns access rights which should be checked for executing GRANT/REVOKE on cluster.
    /// This function is less accurate than checkGrantOption() because it cannot use any information about
    /// access rights the grantees currently have (due to those grantees are located on multiple nodes,
    /// we just don't have the full information about them).
    AccessRightsElements getRequiredAccessForExecutingOnCluster(const AccessRightsElements & elements_to_grant, const AccessRightsElements & elements_to_revoke)
    {
        auto required_access = elements_to_grant;
        required_access.insert(required_access.end(), elements_to_revoke.begin(), elements_to_revoke.end());
        std::for_each(required_access.begin(), required_access.end(), [&](AccessRightsElement & element) { element.grant_option = true; });
        return required_access;
    }

    /// Checks if the current user has enough roles granted with admin option to grant or revoke specified roles on cluster.
    /// This function is less accurate than checkAdminOption() because it cannot use any information about
    /// granted roles the grantees currently have (due to those grantees are located on multiple nodes,
    /// we just don't have the full information about them).
    void checkAdminOptionForExecutingOnCluster(const ContextAccessWrapper & current_user_access,
                                               const std::vector<UUID> roles_to_grant,
                                               const RolesOrUsersSet & roles_to_revoke)
    {
        if (roles_to_revoke.all)
        {
            /// Revoking all the roles on cluster always requires ROLE_ADMIN privilege
            /// because when we send the query REVOKE ALL to each shard we don't know at this point
            /// which roles exactly this is going to revoke on each shard.
            /// However ROLE_ADMIN just allows to revoke every role, that's why we check it here.
            current_user_access.checkAccess(AccessType::ROLE_ADMIN);
            return;
        }

        if (current_user_access.isGranted(AccessType::ROLE_ADMIN))
            return;

        for (const auto & role_id : roles_to_grant)
            current_user_access.checkAdminOption(role_id);


        for (const auto & role_id : roles_to_revoke.getMatchingIDs())
            current_user_access.checkAdminOption(role_id);
    }

    template <typename T>
    void updateGrantedAccessRightsAndRolesTemplate(
        T & grantee,
        const AccessRightsElements & elements_to_grant,
        const AccessRightsElements & elements_to_revoke,
        const std::vector<UUID> & roles_to_grant,
        const RolesOrUsersSet & roles_to_revoke,
        bool admin_option)
    {
        if (!elements_to_revoke.empty())
            grantee.access.revoke(elements_to_revoke);

        if (!elements_to_grant.empty())
            grantee.access.grant(elements_to_grant);

        if (!roles_to_revoke.empty())
        {
            if (admin_option)
                grantee.granted_roles.revokeAdminOption(grantee.granted_roles.findGrantedWithAdminOption(roles_to_revoke));
            else
                grantee.granted_roles.revoke(grantee.granted_roles.findGranted(roles_to_revoke));
        }

        if (!roles_to_grant.empty())
        {
            if (admin_option)
                grantee.granted_roles.grantWithAdminOption(roles_to_grant);
            else
                grantee.granted_roles.grant(roles_to_grant);
        }
    }

    /// Updates grants of a specified user or role.
    void updateGrantedAccessRightsAndRoles(
        IAccessEntity & grantee,
        const AccessRightsElements & elements_to_grant,
        const AccessRightsElements & elements_to_revoke,
        const std::vector<UUID> & roles_to_grant,
        const RolesOrUsersSet & roles_to_revoke,
        bool admin_option)
    {
        if (auto * user = typeid_cast<User *>(&grantee))
            updateGrantedAccessRightsAndRolesTemplate(*user, elements_to_grant, elements_to_revoke, roles_to_grant, roles_to_revoke, admin_option);
        else if (auto * role = typeid_cast<Role *>(&grantee))
            updateGrantedAccessRightsAndRolesTemplate(*role, elements_to_grant, elements_to_revoke, roles_to_grant, roles_to_revoke, admin_option);
    }

    template <typename T>
    void grantCurrentGrantsTemplate(
        T & grantee,
        const AccessRights & rights_to_grant,
        const AccessRightsElements & elements_to_revoke)
    {
        if (!elements_to_revoke.empty())
            grantee.access.revoke(elements_to_revoke);

        grantee.access.makeUnion(rights_to_grant);
    }

    /// Grants current user's grants with grant options to specified user.
    void grantCurrentGrants(
        IAccessEntity & grantee,
        const AccessRights & new_rights,
        const AccessRightsElements & elements_to_revoke)
    {
        if (auto * user = typeid_cast<User *>(&grantee))
            grantCurrentGrantsTemplate(*user, new_rights, elements_to_revoke);
        else if (auto * role = typeid_cast<Role *>(&grantee))
            grantCurrentGrantsTemplate(*role, new_rights, elements_to_revoke);
    }

    /// Calculates all available rights to grant with current user intersection.
    void calculateCurrentGrantRightsWithIntersection(
        AccessRights & rights,
        std::shared_ptr<const ContextAccessWrapper> current_user_access,
        const AccessRightsElements & elements_to_grant)
    {
        AccessRightsElements current_user_grantable_elements;
        auto available_grant_elements = current_user_access->getAccessRights()->getElements();
        AccessRights current_user_rights;
        for (auto & element : available_grant_elements)
        {
            if (!element.grant_option && !element.is_partial_revoke)
                continue;

            if (element.is_partial_revoke)
                current_user_rights.revoke(element);
            else
                current_user_rights.grant(element);
        }

        rights.grant(elements_to_grant);
        rights.makeIntersection(current_user_rights);
    }

    /// Updates grants of a specified user or role.
    void updateFromQuery(IAccessEntity & grantee, const ASTGrantQuery & query)
    {
        AccessRightsElements elements_to_grant;
        AccessRightsElements elements_to_revoke;
        collectAccessRightsElementsToGrantOrRevoke(query, elements_to_grant, elements_to_revoke);

        std::vector<UUID> roles_to_grant;
        RolesOrUsersSet roles_to_revoke;
        collectRolesToGrantOrRevoke(query, roles_to_grant, roles_to_revoke);

        updateGrantedAccessRightsAndRoles(grantee, elements_to_grant, elements_to_revoke, roles_to_grant, roles_to_revoke, query.admin_option);
    }
}


BlockIO InterpreterGrantQuery::execute()
{
    const auto updated_query = removeOnClusterClauseIfNeeded(query_ptr, getContext());
    auto & query = updated_query->as<ASTGrantQuery &>();

    query.replaceCurrentUserTag(getContext()->getUserName());
    query.access_rights_elements.eraseNotGrantable();

    if (!query.access_rights_elements.sameOptions())
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Elements of an ASTGrantQuery are expected to have the same options");
    if (!query.access_rights_elements.empty() && query.access_rights_elements[0].is_partial_revoke && !query.is_revoke)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "A partial revoke should be revoked, not granted");

    auto & access_control = getContext()->getAccessControl();
    auto current_user_access = getContext()->getAccess();

    std::vector<UUID> grantees = RolesOrUsersSet{*query.grantees, access_control, getContext()->getUserID()}.getMatchingIDs(access_control);

    /// Collect access rights and roles we're going to grant or revoke.
    AccessRightsElements elements_to_grant;
    AccessRightsElements elements_to_revoke;
    collectAccessRightsElementsToGrantOrRevoke(query, elements_to_grant, elements_to_revoke);

    std::vector<UUID> roles_to_grant;
    RolesOrUsersSet roles_to_revoke;
    collectRolesToGrantOrRevoke(access_control, query, roles_to_grant, roles_to_revoke);

    /// Replacing empty database with the default. This step must be done before replication to avoid privilege escalation.
    String current_database = getContext()->getCurrentDatabase();
    elements_to_grant.replaceEmptyDatabase(current_database);
    elements_to_revoke.replaceEmptyDatabase(current_database);
    query.access_rights_elements.replaceEmptyDatabase(current_database);

    /// Executing on cluster.
    if (!query.cluster.empty())
    {
        if (query.current_grants)
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "GRANT CURRENT GRANTS can't be executed on cluster.");

        auto required_access = getRequiredAccessForExecutingOnCluster(elements_to_grant, elements_to_revoke);
        checkAdminOptionForExecutingOnCluster(*current_user_access, roles_to_grant, roles_to_revoke);
        current_user_access->checkGranteesAreAllowed(grantees);
        DDLQueryOnClusterParams params;
        params.access_to_check = std::move(required_access);
        return executeDDLQueryOnCluster(updated_query, getContext(), params);
    }

    /// Check if the current user has corresponding access rights granted with grant option.
    bool need_check_grantees_are_allowed = true;
    if (!query.current_grants)
        checkGrantOption(access_control, *current_user_access, grantees, need_check_grantees_are_allowed, elements_to_grant, elements_to_revoke);

    /// Check if the current user has corresponding roles granted with admin option.
    checkAdminOption(access_control, *current_user_access, grantees, need_check_grantees_are_allowed, roles_to_grant, roles_to_revoke, query.admin_option);

    if (need_check_grantees_are_allowed)
        current_user_access->checkGranteesAreAllowed(grantees);

    AccessRights new_rights;
    if (query.current_grants)
        calculateCurrentGrantRightsWithIntersection(new_rights, current_user_access, elements_to_grant);

    /// Update roles and users listed in `grantees`.
    auto update_func = [&](const AccessEntityPtr & entity, const UUID &) -> AccessEntityPtr
    {
        auto clone = entity->clone();
        if (query.current_grants)
            grantCurrentGrants(*clone, new_rights, elements_to_revoke);
        else
            updateGrantedAccessRightsAndRoles(*clone, elements_to_grant, elements_to_revoke, roles_to_grant, roles_to_revoke, query.admin_option);
        return clone;
    };

    access_control.update(grantees, update_func);

    return {};
}


void InterpreterGrantQuery::updateUserFromQuery(User & user, const ASTGrantQuery & query)
{
    updateFromQuery(user, query);
}

void InterpreterGrantQuery::updateRoleFromQuery(Role & role, const ASTGrantQuery & query)
{
    updateFromQuery(role, query);
}

void registerInterpreterGrantQuery(InterpreterFactory & factory)
{
    auto create_fn = [] (const InterpreterFactory::Arguments & args)
    {
        return std::make_unique<InterpreterGrantQuery>(args.query, args.context);
    };
    factory.registerInterpreter("InterpreterGrantQuery", create_fn);
}

}
