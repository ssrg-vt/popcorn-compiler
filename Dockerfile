# Use an official Python runtime as a parent image
FROM ubuntu

ENV POPCORN_DIR /usr/local/popcorn

# Set the working directory to /app
#WORKDIR ${POPCORN_DIR}

# Copy the everything (except src) into the container at /app
COPY aarch64/ $POPCORN_DIR/aarch64
COPY bin/ $POPCORN_DIR/bin
COPY include/ $POPCORN_DIR/include
COPY lib/ $POPCORN_DIR/lib
COPY share/ $POPCORN_DIR/share
COPY x86_64-pc-linux-gnu/ $POPCORN_DIR/x86_64-pc-linux-gnu/
COPY x86_64/ $POPCORN_DIR/x86_64
