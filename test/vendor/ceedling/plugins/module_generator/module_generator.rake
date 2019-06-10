
namespace :module do
  module_root_separator = ":"

  desc "Generate module (source, header and test files)"
  task :create, :module_path do |t, args|
    files = [args[:module_path]] + (args.extras || [])
    optz = { :module_root_path => "" }
    ["dh", "dih", "mch", "mvp", "src", "test"].each do |pat|
      p = files.delete(pat)
      optz[:pattern] = p unless p.nil?
    end
    files.each {
      |v|
      module_root_path, module_name = v.split(module_root_separator, 2)
      if module_name
        optz[:module_root_path] = module_root_path
        v = module_name
      end
      @ceedling[:module_generator].create(v, optz)
    }
  end

  desc "Destroy module (source, header and test files)"
  task :destroy, :module_path do |t, args|
    files = [args[:module_path]] + (args.extras || [])
    optz = { :destroy => true, :module_root_path => "" }
    ["dh", "dih", "mch", "mvp", "src", "test"].each do |pat|
      p = files.delete(pat)
      optz[:pattern] = p unless p.nil?
    end
    files.each {
      |v|
      module_root_path, module_name = v.split(module_root_separator, 2)
      if module_name
        optz[:module_root_path] = module_root_path
        v = module_name
      end
      @ceedling[:module_generator].create(v, optz)
    }
  end

end
