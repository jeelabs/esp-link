# Dockerfile for github.com/jeelabs/esp-link
#
# This dockerfile is intended to be used to compile esp-link as it's checked out on
# your desktop/laptop. You can git clone esp-link, and then compile it using
# a commandline of `docker run -v $PWD:/esp-link jeelabs/esp-link`. The -v mounts
# your esp-link source directory onto /esp-link in the container and the default command is
# to run make.
# If you would like to create your own container image, use `docker build -t esp-link .`
FROM ubuntu:16.04

RUN apt-get update \
 && apt-get install -y software-properties-common build-essential python curl git \
                       zlib1g-dev openjdk-8-jre-headless

RUN curl -Ls http://s3.voneicken.com/xtensa-lx106-elf-20160330.tgx | tar Jxf -
RUN curl -Ls http://s3.voneicken.com/esp_iot_sdk_v2.1.0.tgx | tar -Jxf -

ENV XTENSA_TOOLS_ROOT /xtensa-lx106-elf/bin/

# This could be used to create an image with esp-link in it from github:
#RUN git clone https://github.com/jeelabs/esp-link

# This could be used to create an image with esp-link in it from the local dir:
#COPY . esp-link/

# Expect the esp-link source/home dir to be mounted here:
VOLUME /esp-link
WORKDIR /esp-link

# Default command is to run a build, can be overridden on the docker commandline:
CMD make
