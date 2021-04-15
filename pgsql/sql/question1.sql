-- Question 1
-- I.T.H. Deyzel
--
-- Do not have Oracle installed at home, instead used PostgreSQL
--

--
-- split contract period into correct parts and assign to correct months dividing value by % overlap of charge period
-- eg. 8 month contract for Q2 to Q4, 2, 3 and 3 months with 2/3 * charge for first one
--
-- MM summ is interest per month for all months in contract
-- QQ summ/3 is interest per month for all months in quarter in contract
-- YY summ/12 is interest per month for all months in year falling in contract
--
WITH charge AS (
    SELECT type::CHAR(2),
           start::INT4,
           period::INT4
      FROM (
        VALUES ('YY', 1, 12),
               ('QQ', 1, 3),
               ('MM', 1, 1)
    ) AS m (type, start, period)
),
months AS (
    SELECT month::SMALLINT, name::VARCHAR
      FROM (
        VALUES (1, 'JAN'),
               (2, 'FEB'),
               (3, 'MAR'),
               (4, 'APR'),
               (5, 'MAY'),
               (6, 'JUN'),
               (7, 'JUL'),
               (8, 'AUG'),
               (9, 'SEP'),
               (10, 'OCT'),
               (11, 'NOV'),
               (12, 'DEC')
        ) AS m (month, name)
),
chargeRules AS (
    SELECT m.month,
           c.type,
           m.month % c.period AS seq,
           (1::NUMERIC / c.period::NUMERIC)::NUMERIC AS rate
      FROM charge AS c, months AS m
),
contractCharge AS (
    SELECT c.id, 
           c.period_type, 
           m.name AS month, 
           (c.summ * r.rate)::NUMERIC(20,2) AS charge, 
           c.date_beg AS contract_start, 
           c.date_end AS contract_end, 
           EXTRACT (year FROM date_end) AS year
      FROM (
        SELECT id, 
               date_beg,
               COALESCE(date_end, (EXTRACT(YEAR FROM date_beg)::VARCHAR || '-12-31')::DATE) AS date_end,
               period_type,
               summ
          FROM contracts
      ) AS c
      JOIN chargeRules AS r
        ON c.period_type = r.type AND
           r.month BETWEEN EXTRACT(MONTH FROM date_beg)::SMALLINT AND EXTRACT(MONTH FROM date_end)::SMALLINT
      JOIN months AS m
        ON r.month = m.month
    ORDER BY c.id, r.month
)
-- lazy and inefficient pivot, ran out of time on this question
SELECT c.id AS "ID",
       c.period_type AS "PERIOD_TYPE",
       c.year AS "YEAR",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'JAN') AS "SUM_JAN",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'FEB') AS "SUM_FEB",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'MAR') AS "SUM_MAR",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'APR') AS "SUM_APR",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'MAY') AS "SUM_MAY",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'JUN') AS "SUM_JUN",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'JUL') AS "SUM_JUL",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'AUG') AS "SUM_AUG",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'SEP') AS "SUM_SEP",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'OCT') AS "SUM_OCT",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'NOV') AS "SUM_NOV",
       (SELECT charge FROM contractCharge WHERE id = c.id AND period_type = c.period_type AND month = 'DEC') AS "SUM_DEC"
  FROM contractCharge AS c
GROUP BY c.id, c.period_type, c.year
ORDER BY c.id, c.period_type, c.year

-- 
-- dev=> \i sql/question1.sql 
--  ID | PERIOD_TYPE | YEAR | SUM_JAN | SUM_FEB | SUM_MAR | SUM_APR  | SUM_MAY  | SUM_JUN  | SUM_JUL  | SUM_AUG  | SUM_SEP  | SUM_OCT  | SUM_NOV  | SUM_D
-- EC  
-- ----+-------------+------+---------+---------+---------+----------+----------+----------+----------+----------+----------+----------+----------+------
-- ----
--   1 | MM          | 2012 |  120.00 |  120.00 |  120.00 |   120.00 |   120.00 |   120.00 |   120.00 |   120.00 |   120.00 |   120.00 |   120.00 |   120
-- .00
--   2 | MM          | 2011 | 3000.00 | 3000.00 | 3000.00 |  3000.00 |  3000.00 |  3000.00 |  3000.00 |  3000.00 |  3000.00 |  3000.00 |  3000.00 |  3000
-- .00
--   3 | QQ          | 2012 |         |         |         | 15290.00 | 15290.00 | 15290.00 | 15290.00 | 15290.00 | 15290.00 | 15290.00 | 15290.00 | 15290
-- .00
--   4 | QQ          | 2011 |  411.33 |  411.33 |  411.33 |   411.33 |   411.33 |   411.33 |   411.33 |   411.33 |   411.33 |   411.33 |   411.33 |   411
-- .33
--   5 | MM          | 2012 | 3090.00 | 3090.00 | 3090.00 |  3090.00 |  3090.00 |  3090.00 |  3090.00 |  3090.00 |  3090.00 |  3090.00 |  3090.00 |  3090
-- .00
--   6 | QQ          | 2012 |  286.67 |  286.67 |  286.67 |   286.67 |   286.67 |   286.67 |   286.67 |   286.67 |   286.67 |   286.67 |   286.67 |   286
-- .67
--   7 | YY          | 2011 |  140.00 |  140.00 |  140.00 |   140.00 |   140.00 |   140.00 |   140.00 |   140.00 |   140.00 |   140.00 |   140.00 |   140
-- .00
-- (7 rows)
-- 
