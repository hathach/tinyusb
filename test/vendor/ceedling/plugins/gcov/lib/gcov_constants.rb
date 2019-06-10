
GCOV_ROOT_NAME         = 'gcov'.freeze
GCOV_TASK_ROOT         = GCOV_ROOT_NAME + ':'
GCOV_SYM               = GCOV_ROOT_NAME.to_sym

GCOV_BUILD_PATH        = File.join(PROJECT_BUILD_ROOT, GCOV_ROOT_NAME)
GCOV_BUILD_OUTPUT_PATH = File.join(GCOV_BUILD_PATH, "out")
GCOV_RESULTS_PATH      = File.join(GCOV_BUILD_PATH, "results")
GCOV_DEPENDENCIES_PATH = File.join(GCOV_BUILD_PATH, "dependencies")
GCOV_ARTIFACTS_PATH    = File.join(PROJECT_BUILD_ARTIFACTS_ROOT, GCOV_ROOT_NAME)

GCOV_ARTIFACTS_FILE    = File.join(GCOV_ARTIFACTS_PATH, "GcovCoverageResults.html")

GCOV_IGNORE_SOURCES    = %w(unity cmock cexception).freeze

GCOV_FILTER_EXPR       = '^vendor.*|^build.*|^test.*|^lib.*'


