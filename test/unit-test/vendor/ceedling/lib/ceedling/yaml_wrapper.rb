require 'yaml'
require 'erb'


class YamlWrapper

  def load(filepath)
    return YAML.load(ERB.new(File.read(filepath)).result)
  end

  def dump(filepath, structure)
    File.open(filepath, 'w') do |output|
      YAML.dump(structure, output)
    end
  end

end
