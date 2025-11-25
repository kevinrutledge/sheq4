#!/bin/bash
source .env
scp sheq4.c Makefile test.sh ${UNIX_USER}@${UNIX_HOST}:${UNIX_PATH}/