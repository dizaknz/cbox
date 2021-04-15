-- Question 3
-- I.T.H. Deyzel
--
-- Do not have Oracle installed at home, instead used plpgsql which is very
-- similar to PL/SQL
--
CREATE OR REPLACE FUNCTION WORKDAYS (d1 Date, d2 Date)
RETURNS INTEGER
AS 
$_$
DECLARE
    sdate DATE;
    edate DATE;
    workdays INTEGER;
    startOffset SMALLINT;
    endOffset SMALLINT;
    WEEK SMALLINT DEFAULT 7;
    WORK_WEEK SMALLINT DEFAULT 5;
    done BOOLEAN DEFAULT FALSE;
BEGIN
    IF (d1 IS NULL OR d2 IS NULL)
    THEN
        RAISE EXCEPTION 'dates cannot be null';
    END IF;

    -- if d2 < d1 then negated range
    IF (d2 < d1)
    THEN
        sdate := d2;
        edate := d1;
    ELSE
        sdate := d1;
        edate := d2;
    END IF;

    -- get day in week offsets
    SELECT MOD(EXTRACT(DOW FROM sdate)::SMALLINT, WEEK) INTO startOffset;
    SELECT MOD(EXTRACT(DOW FROM edate)::SMALLINT, WEEK) INTO endOffset;

    -- weekend
    IF startOffset = 0 OR startOffset > WORK_WEEK
    THEN
        startOffset := WORK_WEEK + 1; 
    END IF;

    -- weekend
    IF endOffset = 0 OR endOffset > WORK_WEEK
    THEN
        endOffset := WORK_WEEK;
    END IF;

    IF EXTRACT(WEEK FROM sdate)::SMALLINT = EXTRACT(WEEK FROM edate)::SMALLINT
    THEN
        -- same week, just days
        workdays := endOffset - startOffset + 1;
        done := TRUE;
    END IF;

    -- different weeks
    IF NOT done
    THEN
        -- work days in start/end week
        workdays := (WORK_WEEK - startOffset + 1) + endOffset;
        -- work weeks, non-inclusive
        workdays := workdays + (EXTRACT(WEEK FROM edate)::SMALLINT - EXTRACT(WEEK FROM sdate)::SMALLINT - 1) * WORK_WEEK;
        done := TRUE ;
    END IF;

    RETURN workdays;
END;
$_$
LANGUAGE 'plpgsql' VOLATILE STRICT;

-- 
-- results (run in psql):
-- 
-- dev=> \i question3.sql 
--
--CREATE FUNCTION
--
-- dev=> SELECT workdays (now()::DATE, (now() -  interval '10 days')::DATE);
--
-- workdays 
-- ----------
--        7
-- (1 row)
--
-- dev=> SELECT workdays (now()::DATE, (now() +  interval '10 days')::DATE);
-- workdays 
-- ----------
--        8
-- (1 row)
--


