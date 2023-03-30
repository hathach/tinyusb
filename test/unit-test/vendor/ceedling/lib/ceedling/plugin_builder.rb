require 'ceedling/plugin'

class PluginBuilder

  attr_accessor :plugin_objects

  def construct_plugin(plugin_name, object_map_yaml, system_objects)
    # @streaminator.stdout_puts("Constructing plugin #{plugin_name}...", Verbosity::OBNOXIOUS)
    object_map = {}
    @plugin_objects = {}
    @system_objects = system_objects

    if object_map_yaml
      @object_map = YAML.load(object_map_yaml)
      @object_map.each_key do |obj|
        construct_object(obj)
      end
    else
      raise "Invalid object map for plugin #{plugin_name}!"
    end

    return @plugin_objects
  end

  private

  def camelize(underscored_name)
    return underscored_name.gsub(/(_|^)([a-z0-9])/) {$2.upcase}
  end

  def construct_object(obj)
    if @plugin_objects[obj].nil?
      if @object_map[obj] && @object_map[obj]['compose']
        @object_map[obj]['compose'].each do |dep|
          construct_object(dep)
        end
      end
      build_object(obj)
    end
  end

  def build_object(new_object)
    if @plugin_objects[new_object.to_sym].nil?
      # @streaminator.stdout_puts("Building plugin object #{new_object}", Verbosity::OBNOXIOUS)
      require new_object
      class_name = camelize(new_object)
      new_instance = eval("#{class_name}.new(@system_objects, class_name.to_s)")
      new_instance.plugin_objects = @plugin_objects
      @plugin_objects[new_object.to_sym] = new_instance
    end
  end

end
