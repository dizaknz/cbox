#!/bin/bash
#
# Do not have access to Oracle at home, instead using PostgreSQL for sample code
# This script requires installation of PostgreSQL 9.x+
#
# I.T.H. Deyzel
#

# absolute path to this
BIN=`cd $(dirname $0); pwd`

# globals
readonly PGDB="dbvisit"
readonly PGUSER="sample"
readonly PGPASSWD="s@mpl3"
readonly PGSCHEMA="sample"
readonly PGSUPERUSER="postgres"

# usage 
usage() {
    cat <<EOF

NAME
        $(basename $0) - script to run SQL questions provided by dbVisit

SYNOPSIS
        $(basename $0) -s <server> -h

DESCRIPTION
        -s  PG data server to create sample database and run samples, defaults to localhost
EOF
}

# setup test fixtures
setup() {
    psql -U $PGSUPERUSER -q <<EOF
    -- create role
    CREATE ROLE ${PGUSER} 
    WITH
        NOSUPERUSER
        NOCREATEDB
        NOCREATEROLE
        LOGIN
        CONNECTION LIMIT 50
        ENCRYPTED PASSWORD '${PGPASSWD}';

    -- create db
    CREATE DATABASE ${PGDB}
    WITH 
        OWNER            = ${PGUSER}
        ENCODING         = 'UTF8' 
        TABLESPACE       = pg_default
        CONNECTION LIMIT = 50
        TEMPLATE         = template1;
EOF
    [ $? -ne 0 ] && { echo "FATAL: could not create sample database"; return 1; }

    PGPASSWORD=$PGPASSWD psql -d $PGDB -U $PGUSER -q <<EOF
        -- Question 1:

        DROP TABLE IF EXISTS contracts;
        CREATE TABLE contracts
          (
            id          NUMERIC,
            date_beg    DATE,
            date_end    DATE,
            period_type VARCHAR(2),
            summ        NUMERIC(9,2)
          );
        INSERT
        INTO contracts VALUES
          (
            1,
            to_date('01/01/2012','DD/MM/YYYY') ,
            NULL ,
            'MM',
            120
          );
        INSERT
        INTO contracts VALUES
          (
            2,
            to_date('01/01/2011','DD/MM/YYYY') ,
            to_date('31/12/2011' ,'DD/MM/YYYY') ,
            'MM',
            3000
          );
        INSERT
        INTO contracts VALUES
          (
            3,
            to_date('01/04/2012','DD/MM/YYYY') ,
            NULL ,
            'QQ',
            45870
          );
        INSERT
        INTO contracts VALUES
          (
            4,
            to_date('01/01/2011','DD/MM/YYYY') ,
            NULL ,
            'QQ',
            1234
          );
        INSERT
        INTO contracts VALUES
          (
            5,
            to_date('01/01/2012','DD/MM/YYYY') ,
            NULL ,
            'MM',
            3090
          );
        INSERT
        INTO contracts VALUES
          (
            6,
            to_date('01/01/2012','DD/MM/YYYY') ,
            NULL ,
            'QQ',
            860
          );
        INSERT
        INTO contracts VALUES
          (
            7,
            to_date('01/01/2011','DD/MM/YYYY') ,
            NULL ,
            'YY',
            1680
          );
          
        -- Question 2:

        DROP TABLE IF EXISTS delivers;
        DROP TABLE IF EXISTS services;
        DROP TABLE IF EXISTS turns;

        CREATE TABLE delivers (
            id NUMERIC, 
            name VARCHAR(256)
        );

        CREATE TABLE services (
            id NUMERIC, 
            name VARCHAR(256), 
            deliver NUMERIC
        );

        CREATE TABLE turns (
            id NUMERIC, 
            service NUMERIC, 
            turn NUMERIC(9,2)
        );

        INSERT INTO delivers VALUES (1, 'inflater');
        INSERT INTO delivers VALUES (2, 'bonehead');
        INSERT INTO delivers VALUES (3, 'downer');
        INSERT INTO services VALUES (1, 'inflating balloons', 1);
        INSERT INTO services VALUES (2, 'washing windows', 1);
        INSERT INTO services VALUES (3, 'reading books', 1);
        INSERT INTO services VALUES (4, 'teaching music', 1);
        INSERT INTO services VALUES (5, 'having fun', 2);
        INSERT INTO services VALUES (6, 'cooking dinner', 2);
        INSERT INTO services VALUES (7, 'photo', 2);
        INSERT INTO services VALUES (8, 'Hanging noodles on the ears', 2);
        INSERT INTO services VALUES (9, 'Wall painting nail polish', 3);
        INSERT INTO services VALUES (10, 'honors Certification', 3);
        INSERT INTO services VALUES (11, 'programming', 3);
        INSERT INTO turns VALUES (1, 1, 100);
        INSERT INTO turns VALUES (2, 2, 150);
        INSERT INTO turns VALUES (3, 3, 550);
        INSERT INTO turns VALUES (4, 4, 550);
        INSERT INTO turns VALUES (5, 6, 780);
        INSERT INTO turns VALUES (6, 7, 670);
        INSERT INTO turns VALUES (7, 8, 100);
        INSERT INTO turns VALUES (8, 9, 160);
        INSERT INTO turns VALUES (9, 10, 68550);
        INSERT INTO turns VALUES (10,11, 460);
        INSERT INTO turns VALUES (11,1, 45680);
        INSERT INTO turns VALUES (12,2, 4670);
EOF
    return $?
}

question1() {
    PGPASSWORD=$PGPASSWD psql -d $PGDB -U $PGUSER -q <<EOF
        \i $BIN/sql/question1.sql
EOF
}

question2() {
    PGPASSWORD=$PGPASSWD psql -d $PGDB -U $PGUSER -q <<EOF
        \i $BIN/sql/question2.sql
EOF
}

question3() {
    PGPASSWORD=$PGPASSWD psql -d $PGDB -U $PGUSER -q <<EOF
        \i $BIN/sql/question3.sql
        \df workdays
        SELECT workdays (now()::DATE, (now() -  interval '10 days')::DATE);
        SELECT workdays (now()::DATE, (now() +  interval '10 days')::DATE);
EOF
}

teardown() {
  psql -U $PGSUPERUSER -q <<EOF
    DROP DATABASE ${PGDB};
    DROP ROLE ${PGUSER};
EOF
}

main() {
    setup
    [ $? -ne 0 ] && { teardown; exit 1; }
    echo "INFO: Running question 1"
    question1
    echo "INFO: Running question 2"
    question2
    echo "INFO: Running question 3"
    question3
    teardown
}

main "$@"
