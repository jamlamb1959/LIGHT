BRANCH=$${CI_COMMIT_BRANCH:-main}
ENVS=heltec
PROG=LC

all: push

.build : src/main.cpp platformio.ini Makefile
	pio run
	touch .build

upload.heltec: .build
	pio run -e heltec --target upload

push: .build
	for ENV in $(ENVS) ; do \
        ssh lamb@pharmdata.ddns.net mkdir -p /var/www/html/firmware/$(BRANCH)/$$ENV/$(PROG) ; \
        echo "ENV $$ENV" ; \
	    echo "scp .pio/build/$$ENV/firmware.bin lamb@pharmdata.ddns.net:/var/www/html/firmware/$(BRANCH)/$$ENV/$(PROG)/firmware.bin" ; \
	    scp .pio/build/$$ENV/firmware.bin lamb@pharmdata.ddns.net:/var/www/html/firmware/$(BRANCH)/$$ENV/$(PROG)/firmware.bin ; \
	    done
	mosquitto_pub -h mqtt.lam -p 1883 -t "track/light/MGMT" -m "reboot"

monitor:
	pio device monitor

um: upload.heltec monitor
mu: upload.heltec monitor


