require 'rake'

ERR_MSG = <<EOF
I expected to see a project.yml file in the current directoy. Please create one
by hand or by using the 'ceedling' shell command.
EOF

def ceedling_dir
  File.join(
    File.dirname(__FILE__),
    '..')
end

def builtin_ceedling_plugins_path
  File.join(
    ceedling_dir,
    'plugins')
end

ceeling_lib_rakefile = File.join( ceedling_dir,
                                 'lib',
                                 'rakefile.rb')
if File.exists? "./project.yml"
  load ceeling_lib_rakefile
else
  $stderr.puts ERR_MSG
end
