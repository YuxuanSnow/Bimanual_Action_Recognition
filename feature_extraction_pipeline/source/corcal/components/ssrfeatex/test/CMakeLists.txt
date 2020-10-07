# Libs required for the tests
SET(LIBS ${LIBS} ArmarXCore corcal-ssrfeatex)

armarx_add_test(test-ssrfeatex component_test.cpp "${LIBS}")
