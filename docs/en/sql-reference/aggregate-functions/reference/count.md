---
description: 'Counts the number of rows or not-NULL values.'
sidebar_position: 120
slug: /sql-reference/aggregate-functions/reference/count
title: 'count'
---

# count

Counts the number of rows or not-NULL values.

ClickHouse supports the following syntaxes for `count`:

- `count(expr)` or `COUNT(DISTINCT expr)`.
- `count()` or `COUNT(*)`. The `count()` syntax is ClickHouse-specific.

**Arguments**

The function can take:

- Zero parameters.
- One [expression](/sql-reference/syntax#expressions).

**Returned value**

- If the function is called without parameters it counts the number of rows.
- If the [expression](/sql-reference/syntax#expressions) is passed, then the function counts how many times this expression returned not null. If the expression returns a [Nullable](../../../sql-reference/data-types/nullable.md)-type value, then the result of `count` stays not `Nullable`. The function returns 0 if the expression returned `NULL` for all the rows.

In both cases the type of the returned value is [UInt64](../../../sql-reference/data-types/int-uint.md).

**Details**

ClickHouse supports the `COUNT(DISTINCT ...)` syntax. The behavior of this construction depends on the [count_distinct_implementation](../../../operations/settings/settings.md#count_distinct_implementation) setting. It defines which of the [uniq\*](/sql-reference/aggregate-functions/reference/uniq) functions is used to perform the operation. The default is the [uniqExact](/sql-reference/aggregate-functions/reference/uniqexact) function.

The `SELECT count() FROM table` query is optimized by default using metadata from MergeTree. If you need to use row-level security, disable optimization using the [optimize_trivial_count_query](/operations/settings/settings#optimize_trivial_count_query) setting.

However `SELECT count(nullable_column) FROM table` query can be optimized by enabling the [optimize_functions_to_subcolumns](/operations/settings/settings#optimize_functions_to_subcolumns) setting. With `optimize_functions_to_subcolumns = 1` the function reads only [null](../../../sql-reference/data-types/nullable.md#finding-null) subcolumn instead of reading and processing the whole column data. The query `SELECT count(n) FROM table` transforms to `SELECT sum(NOT n.null) FROM table`.

**Improving COUNT(DISTINCT expr) performance**

If your `COUNT(DISTINCT expr)` query is slow, consider adding a [`GROUP BY`](/sql-reference/statements/select/group-by) clause as this improves parallelization. You can also use a [projection](../../../sql-reference/statements/alter/projection.md) to create an index on the target column used with `COUNT(DISTINCT target_col)`.

**Examples**

Example 1:

```sql
SELECT count() FROM t
```

```text
┌─count()─┐
│       5 │
└─────────┘
```

Example 2:

```sql
SELECT name, value FROM system.settings WHERE name = 'count_distinct_implementation'
```

```text
┌─name──────────────────────────┬─value─────┐
│ count_distinct_implementation │ uniqExact │
└───────────────────────────────┴───────────┘
```

```sql
SELECT count(DISTINCT num) FROM t
```

```text
┌─uniqExact(num)─┐
│              3 │
└────────────────┘
```

This example shows that `count(DISTINCT num)` is performed by the `uniqExact` function according to the `count_distinct_implementation` setting value.
