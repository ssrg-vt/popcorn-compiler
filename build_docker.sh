#!/bin/bash

install_path="/usr/local/popcorn"

#Build
docker build -t popcorn/compiler -f Dockerfile  $install_path

#Tag
branch_name=$(git branch | grep \* | cut -d ' ' -f2)
commit_id=$(git rev-parse --short HEAD)
tag_id="$branch_name-$commit_id"
docker tag popcorn/compiler:$tag_id 121389845380.dkr.ecr.us-east-1.amazonaws.com/popcorn/compiler:criu-dev-f579b3c90d5

#Push
docker push 121389845380.dkr.ecr.us-east-1.amazonaws.com/popcorn/compiler


