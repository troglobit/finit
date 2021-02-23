AM_TESTS_ENVIRONMENT = make -C test_root/;
TESTS		     = checkself.sh
TESTS		     += test_add_remove_dynamic_service/test.sh
TESTS		     += test_add_remove_dynamic_service_sub_config/test.sh
TESTS                += test_start_stop_service/test.sh
TESTS                += test_start_stop_service_sub_config/test.sh

clean-local:
	make -C test_root/ clean
