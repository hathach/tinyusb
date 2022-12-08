
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
    files.each do |v|
      module_root_path, module_name = v.split(module_root_separator, 2)
      if module_name
        optz[:module_root_path] = module_root_path
        v = module_name
      end
      if (v =~ /^test_?/i)
        # If the name of the file starts with test, automatically treat it as one
        @ceedling[:module_generator].create(v.sub(/^test_?/i,''), optz.merge({:pattern => 'test'}))
      else
        # Otherwise, go through the normal procedure
        @ceedling[:module_generator].create(v, optz)
      end
    end
  end

  desc "Generate module stubs from header"
  task :stub, :module_path do |t, args|
    files = [args[:module_path]] + (args.extras || [])
    optz = { :module_root_path => "" }
    files.each do |v|
      module_root_path, module_name = v.split(module_root_separator, 2)
      if module_name
        optz[:module_root_path] = module_root_path
        v = module_name
      end
      # Otherwise, go through the normal procedure
      @ceedling[:module_generator].stub_from_header(v, optz)
    end
  end

  desc "Destroy module (source, header and test files)"
  task :destroy, :module_path do |t, args|
    files = [args[:module_path]] + (args.extras || [])
    optz = { :destroy => true, :module_root_path => "" }
    ["dh", "dih", "mch", "mvp", "src", "test"].each do |pat|
      p = files.delete(pat)
      optz[:pattern] = p unless p.nil?
    end
    files.each do |v|
      module_root_path, module_name = v.split(module_root_separator, 2)
      if module_name
        optz[:module_root_path] = module_root_path
        v = module_name
      end
      @ceedling[:module_generator].create(v, optz)
    end
  end

end
