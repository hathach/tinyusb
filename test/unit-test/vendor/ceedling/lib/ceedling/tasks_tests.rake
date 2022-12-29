require 'ceedling/constants'

task :test_deps => [:directories]

task :test => [:test_deps] do
  Rake.application['test:all'].invoke
end

namespace TEST_SYM do

  desc "Run all unit tests (also just 'test' works)."
  task :all => [:test_deps] do
    @ceedling[:test_invoker].setup_and_invoke(COLLECTION_ALL_TESTS)
  end

  desc "Run single test ([*] real test or source file name, no path)."
  task :* do
    message = "\nOops! '#{TEST_ROOT_NAME}:*' isn't a real task. " +
              "Use a real test or source file name (no path) in place of the wildcard.\n" +
              "Example: rake #{TEST_ROOT_NAME}:foo.c\n\n"

    @ceedling[:streaminator].stdout_puts( message )
  end

  desc "Run tests for changed files."
  task :delta => [:test_deps] do
    @ceedling[:test_invoker].setup_and_invoke(COLLECTION_ALL_TESTS, TEST_SYM, {:force_run => false})
  end

  desc "Just build tests without running."
  task :build_only => [:test_deps] do
    @ceedling[:test_invoker].setup_and_invoke(COLLECTION_ALL_TESTS, TEST_SYM, {:build_only => true})
  end

  desc "Run tests by matching regular expression pattern."
  task :pattern, [:regex] => [:test_deps] do |t, args|
    matches = []

    COLLECTION_ALL_TESTS.each { |test| matches << test if (test =~ /#{args.regex}/) }

    if (matches.size > 0)
      @ceedling[:test_invoker].setup_and_invoke(matches, TEST_SYM, {:force_run => false})
    else
      @ceedling[:streaminator].stdout_puts("\nFound no tests matching pattern /#{args.regex}/.")
    end
  end

  desc "Run tests whose test path contains [dir] or [dir] substring."
  task :path, [:dir] => [:test_deps] do |t, args|
    matches = []

    COLLECTION_ALL_TESTS.each { |test| matches << test if File.dirname(test).include?(args.dir.gsub(/\\/, '/')) }

    if (matches.size > 0)
      @ceedling[:test_invoker].setup_and_invoke(matches, TEST_SYM, {:force_run => false})
    else
      @ceedling[:streaminator].stdout_puts("\nFound no tests including the given path or path component.")
    end
  end

end

