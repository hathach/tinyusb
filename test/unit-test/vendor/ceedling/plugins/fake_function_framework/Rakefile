require 'rake'
require 'rspec/core/rake_task'

desc "Run all rspecs"
RSpec::Core::RakeTask.new(:spec) do |t|
    t.pattern = Dir.glob('spec/**/*_spec.rb')
    t.rspec_opts = '--format documentation'
    # t.rspec_opts << ' more options'
end

desc "Run integration test on example"
task :integration_test do
    chdir("./examples/fff_example") do
        sh "rake clobber"
        sh "rake test:all"
    end
end

task :default => [:spec, :integration_test]
