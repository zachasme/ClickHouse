SELECT 1, 2 INTO OUTFILE '/dev/null' TRUNCATE FORMAT Npy;       -- { clientError TOO_MANY_COLUMNS }
SELECT 1    INTO OUTFILE '/dev/null' TRUNCATE FORMAT Template;  -- { clientError INVALID_TEMPLATE_FORMAT }
SELECT 'a'  INTO OUTFILE '/dev/null' TRUNCATE FORMAT Avro;      -- { clientError STD_EXCEPTION, UNKNOWN_FORMAT }
