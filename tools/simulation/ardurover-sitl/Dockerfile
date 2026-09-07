FROM ardupilot/ardupilot-dev-base:v0.2.0

ARG ARDUPILOT_TAG=Rover-4.7.0

RUN git clone --branch "${ARDUPILOT_TAG}" --depth 1 https://github.com/ArduPilot/ardupilot.git /ardupilot

WORKDIR /ardupilot

RUN git submodule update --init --recursive --depth 1 \
    && ./waf configure --board sitl \
    && ./waf rover

EXPOSE 5760/tcp

ENTRYPOINT ["/ardupilot/Tools/autotest/sim_vehicle.py"]
