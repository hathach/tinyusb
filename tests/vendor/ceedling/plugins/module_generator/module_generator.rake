
namespace :module do

  desc "Generate module (source, header and test files)"
    task :create, :module_path do |t, args|
    @ceedling[:module_generator].create(args[:module_path])
  end

  desc "Destroy module (source, header and test files)"
  task :destroy, :module_path do |t, args|
    @ceedling[:module_generator].create(args[:module_path], {:destroy => true})
  end
  
end
