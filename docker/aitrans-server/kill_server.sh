#!/bin/bash
kill `lsof -i:5555 | awk '/server/ {print$2}'`