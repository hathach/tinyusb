require 'yaml'


class YamlWrapper

  def load(filepath)
    return YAML.load(File.read(filepath))
  end

  def dump(filepath, structure)
    File.open(filepath, 'w') do |output|
      YAML.dump(structure, output)
    end
  end

end
