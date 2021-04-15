-- Question 2
-- I.T.H. Deyzel
--
-- Do not have Oracle installed at home, instead used PostgreSQL
--
SELECT deliverer, service, turn
  FROM (
    SELECT d.name AS deliverer, 
           s.name AS service, 
           t.turn AS turn, 
           ROW_NUMBER() OVER(PARTITION BY d.name ORDER BY t.turn DESC) AS row
      FROM delivers AS d 
      JOIN services AS s 
        ON d.id = s.deliver
      JOIN turns AS t
        ON s.id = t.service
   ) AS proc
 WHERE row <= 3
ORDER by deliverer, turn DESC;

-- 
-- proc: 
-- subquery to select all deliverers, their services and turn over per service
-- the result set is assigned a row number per deliverer based on their turn over for a service
-- using row_number() over() partitioning result set on deliverer and sorting each sub group
-- of services provided by deliverer on turn over amount in descending order
-- 

-- 
-- results (run in psql):
-- 
-- dev=> \i question2.sql 
--
--  deliverer |           service           |   turn   
-- -----------+-----------------------------+----------
--  bonehead  | cooking dinner              |   780.00
--  bonehead  | photo                       |   670.00
--  bonehead  | Hanging noodles on the ears |   100.00
--  downer    | honors Certification        | 68550.00
--  downer    | programming                 |   460.00
--  downer    | Wall painting nail polish   |   160.00
--  inflater  | inflating balloons          | 45680.00
--  inflater  | washing windows             |  4670.00
--  inflater  | teaching music              |   550.00
--
-- (9 rows)
-- 
