ceedling-gcov
=============

# Plugin Overview

Plugin for integrating GNU GCov code coverage tool into Ceedling projects.
Currently only designed for the gcov command (like LCOV for example). In the
future we could configure this to work with other code coverage tools.

This plugin currently uses [gcovr](https://www.gcovr.com/) and / or
[ReportGenerator](https://danielpalme.github.io/ReportGenerator/)
as utilities to generate HTML, XML, JSON, or Text reports. The normal gcov
plugin _must_ be run first for these reports to generate.

## Installation

gcovr can be installed via pip like so:

```sh
pip install gcovr
```

ReportGenerator can be installed via .NET Core like so:

```sh
dotnet tool install -g dotnet-reportgenerator-globaltool
```

It is not required to install both `gcovr` and `ReportGenerator`. Either utility
may be installed to create reports.

## Configuration

The gcov plugin supports configuration options via your `project.yml` provided
by Ceedling.

### Utilities

Gcovr and / or ReportGenerator may be enabled to create coverage reports.

```yaml
:gcov:
  :utilities:
    - gcovr           # Use gcovr to create the specified reports (default).
    - ReportGenerator # Use ReportGenerator to create the specified reports.
```

### Reports

Various reports are available and may be enabled with the following
configuration item. See the specific report sections in this README
for additional options and information. All generated reports will be found in `build/artifacts/gcov`.

```yaml
:gcov:
  # Specify one or more reports to generate.
  # Defaults to HtmlBasic.
  :reports:
    # Make an HTML summary report.
    # Supported utilities: gcovr, ReportGenerator
    - HtmlBasic

    # Make an HTML report with line by line coverage of each source file.
    # Supported utilities: gcovr, ReportGenerator
    - HtmlDetailed

    # Make a Text report, which may be output to the console with gcovr or a file in both gcovr and ReportGenerator.
    # Supported utilities: gcovr, ReportGenerator
    - Text

    # Make a Cobertura XML report.
    # Supported utilities: gcovr, ReportGenerator
    - Cobertura

    # Make a SonarQube XML report.
    # Supported utilities: gcovr, ReportGenerator
    - SonarQube

    # Make a JSON report.
    # Supported utilities: gcovr
    - JSON

    # Make a detailed HTML report with CSS and JavaScript included in every HTML page. Useful for build servers.
    # Supported utilities: ReportGenerator
    - HtmlInline

    # Make a detailed HTML report with a light theme and CSS and JavaScript included in every HTML page for Azure DevOps.
    # Supported utilities: ReportGenerator
    - HtmlInlineAzure

    # Make a detailed HTML report with a dark theme and CSS and JavaScript included in every HTML page for Azure DevOps.
    # Supported utilities: ReportGenerator
    - HtmlInlineAzureDark

    # Make a single HTML file containing a chart with historic coverage information.
    # Supported utilities: ReportGenerator
    - HtmlChart

    # Make a detailed HTML report in a single file.
    # Supported utilities: ReportGenerator
    - MHtml

    # Make SVG and PNG files that show line and / or branch coverage information.
    # Supported utilities: ReportGenerator
    - Badges

    # Make a single CSV file containing coverage information per file.
    # Supported utilities: ReportGenerator
    - CsvSummary

    # Make a single TEX file containing a summary for all files and detailed reports for each files.
    # Supported utilities: ReportGenerator
    - Latex

    # Make a single TEX file containing a summary for all files.
    # Supported utilities: ReportGenerator
    - LatexSummary

    # Make a single PNG file containing a chart with historic coverage information.
    # Supported utilities: ReportGenerator
    - PngChart

    # Command line output interpreted by TeamCity.
    # Supported utilities: ReportGenerator
    - TeamCitySummary

    # Make a text file in lcov format.
    # Supported utilities: ReportGenerator
    - lcov

    # Make a XML file containing a summary for all classes and detailed reports for each class.
    # Supported utilities: ReportGenerator
    - Xml

    # Make a single XML file containing a summary for all files.
    # Supported utilities: ReportGenerator
    - XmlSummary
```

### Gcovr HTML Reports

Generation of Gcovr HTML reports may be modified with the following configuration items.

```yaml
:gcov:
  # Set to 'true' to enable HTML reports or set to 'false' to disable.
  # Defaults to enabled. (gcovr --html)
  # Deprecated - See the :reports: configuration option.
  :html_report: [true|false]

  # Gcovr supports generating two types of HTML reports. Use 'basic' to create
  # an HTML report with only the overall file information. Use 'detailed' to create
  # an HTML report with line by line coverage of each source file.
  # Defaults to 'basic'. Set to 'detailed' for (gcovr --html-details).
  # Deprecated - See the :reports: configuration option.
  :html_report_type: [basic|detailed]


  :gcovr:
    # HTML report filename.
    :html_artifact_filename: <output>

    # Use 'title' as title for the HTML report.
    # Default is 'Head'. (gcovr --html-title)
    :html_title: <title>

    # If the coverage is below MEDIUM, the value is marked as low coverage in the HTML report.
    # MEDIUM has to be lower than or equal to value of html_high_threshold.
    # If MEDIUM is equal to value of html_high_threshold the report has only high and low coverage.
    # Default is 75.0. (gcovr --html-medium-threshold)
    :html_medium_threshold: 75

    # If the coverage is below HIGH, the value is marked as medium coverage in the HTML report.
    # HIGH has to be greater than or equal to value of html_medium_threshold.
    # If HIGH is equal to value of html_medium_threshold the report has only high and low coverage.
    # Default is 90.0. (gcovr -html-high-threshold)
    :html_high_threshold: 90

    # Set to 'true' to use absolute paths to link the 'detailed' reports.
    # Defaults to relative links. (gcovr --html-absolute-paths)
    :html_absolute_paths: [true|false]

    # Override the declared HTML report encoding. Defaults to UTF-8. (gcovr --html-encoding)
    :html_encoding: <html_encoding>
```

### Cobertura XML Reports

Generation of Cobertura XML reports may be modified with the following configuration items.

```yaml
:gcov:
  # Set to 'true' to enable Cobertura XML reports or set to 'false' to disable.
  # Defaults to disabled. (gcovr --xml)
  # Deprecated - See the :reports: configuration option.
  :xml_report: [true|false]


  :gcovr:
    # Set to 'true' to pretty-print the Cobertura XML report, otherwise set to 'false'.
    # Defaults to disabled. (gcovr --xml-pretty)
    :xml_pretty: [true|false]
    :cobertura_pretty: [true|false]

    # Cobertura XML report filename.
    :xml_artifact_filename: <output>
    :cobertura_artifact_filename: <output>
```

### SonarQube XML Reports

Generation of SonarQube XML reports may be modified with the following configuration items.

```yaml
:gcov:
  :gcovr:
    # SonarQube XML report filename.
    :sonarqube_artifact_filename: <output>
```

### JSON Reports

Generation of JSON reports may be modified with the following configuration items.

```yaml
:gcov:
  :gcovr:
    # Set to 'true' to pretty-print the JSON report, otherwise set 'false'.
    # Defaults to disabled. (gcovr --json-pretty)
    :json_pretty: [true|false]

    # JSON report filename.
    :json_artifact_filename: <output>
```

### Text Reports

Generation of text reports may be modified with the following configuration items.
Text reports may be printed to the console or output to a file.

```yaml
:gcov:
  :gcovr:
    # Text report filename.
    # The text report is printed to the console when no filename is provided.
    :text_artifact_filename: <output>
```

### Common Report Options

There are a number of options to control which files are considered part of
the coverage report. Most often, we only care about coverage on our source code, and not
on tests or automatically generated mocks, runners, etc. However, there are times
where this isn't true... or there are times where we've moved ceedling's directory
structure so that the project file isn't at the root of the project anymore. In these
cases, you may need to tweak `report_include`, `report_exclude`, and `exclude_directories`.

One important note about `report_root`: gcovr will take only a single root folder, unlike
Ceedling's ability to take as many as you like. So you will need to choose a folder which is
a superset of ALL the folders you want, and then use the include or exclude options to set up
patterns of files to pay attention to or ignore. It's not ideal, but it works.

Finally, there are a number of settings which can be specified to adjust the
default behaviors of gcovr:

```yaml
:gcov:
  :gcovr:
    # The root directory of your source files. Defaults to ".", the current directory.
    # File names are reported relative to this root. The report_root is the default report_include.
    :report_root: "."

    # Load the specified configuration file.
    # Defaults to gcovr.cfg in the report_root directory. (gcovr --config)
    :config_file: <config_file>

    # Exit with a status of 2 if the total line coverage is less than MIN.
    # Can be ORed with exit status of 'fail_under_branch' option. (gcovr --fail-under-line)
    :fail_under_line: 30

    # Exit with a status of 4 if the total branch coverage is less than MIN.
    # Can be ORed with exit status of 'fail_under_line' option. (gcovr --fail-under-branch)
    :fail_under_branch: 30

    # Select the source file encoding.
    # Defaults to the system default encoding (UTF-8). (gcovr --source-encoding)
    :source_encoding: <source_encoding>

    # Report the branch coverage instead of the line coverage. For text report only. (gcovr --branches).
    :branches: [true|false]

    # Sort entries by increasing number of uncovered lines.
    # For text and HTML report. (gcovr --sort-uncovered)
    :sort_uncovered: [true|false]

    # Sort entries by increasing percentage of uncovered lines.
    # For text and HTML report. (gcovr --sort-percentage)
    :sort_percentage: [true|false]

    # Print a small report to stdout with line & branch percentage coverage.
    # This is in addition to other reports. (gcovr --print-summary).
    :print_summary: [true|false]

    # Keep only source files that match this filter. (gcovr --filter).
    :report_include: "^src"

    # Exclude source files that match this filter. (gcovr --exclude).
    :report_exclude: "^vendor.*|^build.*|^test.*|^lib.*"

    # Keep only gcov data files that match this filter. (gcovr --gcov-filter).
    :gcov_filter: <gcov_filter>

    # Exclude gcov data files that match this filter. (gcovr --gcov-exclude).
    :gcov_exclude: <gcov_exclude>

    # Exclude directories that match this regex while searching
    # raw coverage files. (gcovr --exclude-directories).
    :exclude_directories: <exclude_dirs>

    # Use a particular gcov executable. (gcovr --gcov-executable).
    :gcov_executable: <gcov_cmd>

    # Exclude branch coverage from lines without useful
    # source code. (gcovr --exclude-unreachable-branches).
    :exclude_unreachable_branches: [true|false]

    # For branch coverage, exclude branches that the compiler
    # generates for exception handling. (gcovr --exclude-throw-branches).
    :exclude_throw_branches: [true|false]

    # Use existing gcov files for analysis. Default: False. (gcovr --use-gcov-files)
    :use_gcov_files: [true|false]

    # Skip lines with parse errors in GCOV files instead of
    # exiting with an error. (gcovr --gcov-ignore-parse-errors).
    :gcov_ignore_parse_errors: [true|false]

    # Override normal working directory detection. (gcovr --object-directory)
    :object_directory: <objdir>

    # Keep gcov files after processing. (gcovr --keep).
    :keep: [true|false]

    # Delete gcda files after processing. (gcovr --delete).
    :delete: [true|false]

    # Set the number of threads to use in parallel. (gcovr -j).
    :num_parallel_threads: <num_threads>

  # When scanning the code coverage, if any files are found that do not have
  # associated coverage data, the command will abort with an error message.
  :abort_on_uncovered: true

  # When using the ``abort_on_uncovered`` option, the files in this list will not
  # trigger a failure.
  # Ceedling globs described in the Ceedling packet ``Path`` section can be used
  # when directories are placed on the list. Globs are limited to matching directories
  # and not files.
  :uncovered_ignore_list: []
```

### ReportGenerator Configuration

The ReportGenerator utility may be configured with the following configuration items.
All generated reports may be found in `build/artifacts/gcov/ReportGenerator`.

```yaml
:gcov:
  :report_generator:
    # Optional directory for storing persistent coverage information.
    # Can be used in future reports to show coverage evolution.
    :history_directory: <history_directory>

    # Optional plugin files for custom reports or custom history storage (separated by semicolon).
    :plugins: CustomReports.dll

    # Optional list of assemblies that should be included or excluded in the report (separated by semicolon)..
    # Exclusion filters take precedence over inclusion filters.
    # Wildcards are allowed, but not regular expressions.
    :assembly_filters: "+Included;-Excluded"

    # Optional list of classes that should be included or excluded in the report (separated by semicolon)..
    # Exclusion filters take precedence over inclusion filters.
    # Wildcards are allowed, but not regular expressions.
    :class_filters: "+Included;-Excluded"

    # Optional list of files that should be included or excluded in the report (separated by semicolon)..
    # Exclusion filters take precedence over inclusion filters.
    # Wildcards are allowed, but not regular expressions.
    :file_filters: "-./vendor/*;-./build/*;-./test/*;-./lib/*;+./src/*"

    # The verbosity level of the log messages.
    # Values: Verbose, Info, Warning, Error, Off
    :verbosity: Warning

    # Optional tag or build version.
    :tag: <tag>

    # Optional list of one or more regular expressions to exclude gcov notes files that match these filters.
    :gcov_exclude:
      - <exclude_regex1>
      - <exclude_regex2>

    # Optionally use a particular gcov executable. Defaults to gcov.
    :gcov_executable: <gcov_cmd>

    # Optionally set the number of threads to use in parallel. Defaults to 1.
    :num_parallel_threads: <num_threads>

    # Optional list of one or more command line arguments to pass to Report Generator.
    # Useful for configuring Risk Hotspots and Other Settings.
    # https://github.com/danielpalme/ReportGenerator/wiki/Settings
    :custom_args:
      - <custom_arg1>
      - <custom_arg2>
```

## Example Usage

```sh
ceedling gcov:all utils:gcov
```

## To-Do list

- Generate overall report (combined statistics from all files with coverage)

## Citations

Most of the comment text which describes the options was taken from the
[Gcovr User Guide](https://www.gcovr.com/en/stable/guide.html) and the
[ReportGenerator Wiki](https://github.com/danielpalme/ReportGenerator/wiki).
The text is repeated here to provide the most accurate option functionality.
