
class PluginManagerHelper

  def include?(plugins, name)
		include = false
		plugins.each do |plugin|
			if (plugin.name == name)
				include = true
				break
			end
		end
		return include
  end

  def instantiate_plugin_script(plugin, system_objects, name)
    return eval("#{plugin}.new(system_objects, name)")
  end

end
