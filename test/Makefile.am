AM_TESTS_ENVIRONMENT = make --silent --no-print-directory -C test-root/;

TESTS                = checkself.sh
TESTS                += add-remove-dynamic-service/test.sh
TESTS                += add-remove-dynamic-service-sub-config/test.sh
TESTS                += start-stop-service/test.sh
TESTS                += start-stop-service-sub-config/test.sh

clean-local:
	make -C test-root/ clean
