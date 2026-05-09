FROM alpine:latest

RUN apk add --no-cache \
   build-base \
   g++ \
   make \
   armadillo-dev \
   fftw \
   fftw-dev \
   py3-mpmath \
   lv2-dev

RUN mkdir -p /usr/local/include/lv2/lv2plug.in/ns/lv2core && \
    ln -sf /usr/include/lv2/core/lv2.h /usr/local/include/lv2/lv2plug.in/ns/lv2core/lv2.h

WORKDIR /app

CMD ["make"]
