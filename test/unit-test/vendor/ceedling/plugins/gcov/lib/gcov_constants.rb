
GCOV_ROOT_NAME                  = 'gcov'.freeze
GCOV_TASK_ROOT                  = GCOV_ROOT_NAME + ':'
GCOV_SYM                        = GCOV_ROOT_NAME.to_sym

GCOV_BUILD_PATH                 = File.join(PROJECT_BUILD_ROOT, GCOV_ROOT_NAME)
GCOV_BUILD_OUTPUT_PATH          = File.join(GCOV_BUILD_PATH, "out")
GCOV_RESULTS_PATH               = File.join(GCOV_BUILD_PATH, "results")
GCOV_DEPENDENCIES_PATH          = File.join(GCOV_BUILD_PATH, "dependencies")
GCOV_ARTIFACTS_PATH             = File.join(PROJECT_BUILD_ARTIFACTS_ROOT, GCOV_ROOT_NAME)
GCOV_REPORT_GENERATOR_PATH      = File.join(GCOV_ARTIFACTS_PATH, "ReportGenerator")

GCOV_ARTIFACTS_FILE_HTML        = File.join(GCOV_ARTIFACTS_PATH, "GcovCoverageResults.html")
GCOV_ARTIFACTS_FILE_COBERTURA   = File.join(GCOV_ARTIFACTS_PATH, "GcovCoverageCobertura.xml")
GCOV_ARTIFACTS_FILE_SONARQUBE   = File.join(GCOV_ARTIFACTS_PATH, "GcovCoverageSonarQube.xml")
GCOV_ARTIFACTS_FILE_JSON        = File.join(GCOV_ARTIFACTS_PATH, "GcovCoverage.json")

GCOV_FILTER_EXCLUDE_PATHS       = ['vendor', 'build', 'test', 'lib']

# gcovr supports regular expressions.
GCOV_FILTER_EXCLUDE = GCOV_FILTER_EXCLUDE_PATHS.map{|path| '^'.concat(*path).concat('.*')}.join('|')

# ReportGenerator supports text with wildcard characters.
GCOV_REPORT_GENERATOR_FILE_FILTERS = GCOV_FILTER_EXCLUDE_PATHS.map{|path| File.join('-.', *path, '*')}.join(';')

# Report Types
class ReportTypes
  HTML_BASIC = "HtmlBasic"
  HTML_DETAILED = "HtmlDetailed"
  HTML_CHART = "HtmlChart"
  HTML_INLINE = "HtmlInline"
  HTML_INLINE_AZURE = "HtmlInlineAzure"
  HTML_INLINE_AZURE_DARK = "HtmlInlineAzureDark"
  MHTML = "MHtml"
  TEXT = "Text"
  COBERTURA = "Cobertura"
  SONARQUBE = "SonarQube"
  JSON = "JSON"
  BADGES = "Badges"
  CSV_SUMMARY = "CsvSummary"
  LATEX = "Latex"
  LATEX_SUMMARY = "LatexSummary"
  PNG_CHART = "PngChart"
  TEAM_CITY_SUMMARY = "TeamCitySummary"
  LCOV = "lcov"
  XML = "Xml"
  XML_SUMMARY = "XmlSummary"
end
