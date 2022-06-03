FROM ubuntu:20.04

# docker build -t arthurpham/flatb-compiler-and-interpreter .
# docker push arthurpham/flatb-compiler-and-interpreter
# docker run arthurpham/flatb-compiler-and-interpreter
# docker run -it arthurpham/flatb-compiler-and-interpreter bash

RUN apt-get update \
  && apt-get install -y build-essential \
  && apt-get install -y flex-old \
  && apt-get install -y bison \
  && apt-get install -y llvm \
  && apt-get install -y time \
  && rm -rf /var/lib/apt/lists/*

COPY . /usr/
WORKDIR /usr/src

RUN make

CMD ["sh", "-c", "./sample.sh"]
