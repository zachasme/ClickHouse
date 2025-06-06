SET output_format_pretty_color = 1, output_format_pretty_max_column_name_width_cut_to = 16;

SELECT CAST((1, 'Hello') AS Tuple(a UInt64, b String)) AS `абвгдежзийклмнопрстуф`, 'Hello' AS x, 'World' AS "абвгдежзийклмнопрстуфхцчшщъыьэюя" FORMAT Pretty;
SELECT CAST((1, 'Hello') AS Tuple(a UInt64, b String)) AS `абвгдежзийклмнопрстуф`, 'Hello' AS x, 'World' AS "абвгдежзийклмнопрстуфхцчшщъыьэюя" FORMAT PrettyCompact;
SELECT CAST((1, 'Hello') AS Tuple(a UInt64, b String)) AS `абвгдежзийклмнопрстуф`, 'Hello' AS x, 'World' AS "абвгдежзийклмнопрстуфхцчшщъыьэюя" FORMAT PrettySpace;
SELECT CAST((1, 'Hello') AS Tuple(a UInt64, b String)) AS `абвгдежзийклмнопрстуф`, 'Hello' AS x, 'World' AS "абвгдежзийклмнопрстуфхцчшщъыьэюя" FORMAT Vertical;

SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщъыьэюя" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщъыьэю" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщъыьэ" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщъыь" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщъы" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщъ" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщ" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчш" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхцч" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфхц" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуфх" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрстуф" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрсту" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрст" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопрс" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмнопр" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмноп" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмно" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклмн" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийклм" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийкл" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзийк" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзий" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежзи" FORMAT PrettyCompact;
SELECT 1 AS "абвгдежз" FORMAT PrettyCompact;
SELECT 1 AS "абвгдеж" FORMAT PrettyCompact;
SELECT 1 AS "абвгде" FORMAT PrettyCompact;
SELECT 1 AS "абвгд" FORMAT PrettyCompact;
SELECT 1 AS "абвг" FORMAT PrettyCompact;
SELECT 1 AS "абв" FORMAT PrettyCompact;
SELECT 1 AS "аб" FORMAT PrettyCompact;
SELECT 1 AS "а" FORMAT PrettyCompact;

SELECT 1 AS "@abcdefghijklmnopqrstuvxyz" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopqrstuvxy" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopqrstuvx" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopqrstuv" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopqrstu" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopqrst" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopqrs" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopqr" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnopq" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmnop" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmno" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklmn" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijklm" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijkl" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghijk" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghij" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefghi" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefgh" FORMAT PrettyCompact;
SELECT 1 AS "@abcdefg" FORMAT PrettyCompact;
SELECT 1 AS "@abcdef" FORMAT PrettyCompact;
SELECT 1 AS "@abcde" FORMAT PrettyCompact;
SELECT 1 AS "@abcd" FORMAT PrettyCompact;
SELECT 1 AS "@abc" FORMAT PrettyCompact;
SELECT 1 AS "@ab" FORMAT PrettyCompact;
SELECT 1 AS "@a" FORMAT PrettyCompact;

SET output_format_pretty_max_column_name_width_cut_to = 0;
SELECT 1 AS "абвгдежзийклмнопрстуфхцчшщъыьэюя" FORMAT PrettyCompact;
