# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ========================================== 

class CMockPluginManager

  attr_accessor :plugins
  
  def initialize(config, utils)
    @plugins = []
    plugins_to_load = [:expect, config.plugins].flatten.uniq.compact
    plugins_to_load.each do |plugin|
      plugin_name = plugin.to_s
      object_name = "CMockGeneratorPlugin" + camelize(plugin_name)
      begin
        unless (Object.const_defined? object_name)
          require "#{File.expand_path(File.dirname(__FILE__))}/cmock_generator_plugin_#{plugin_name.downcase}.rb"
        end
        @plugins << eval("#{object_name}.new(config, utils)")
      rescue
        raise "ERROR: CMock unable to load plugin '#{plugin_name}'"
      end
    end
    @plugins.sort! {|a,b| a.priority <=> b.priority }
  end
  
  def run(method, args=nil)
    if args.nil?
      return @plugins.collect{ |plugin| plugin.send(method) if plugin.respond_to?(method) }.flatten.join
    else
      return @plugins.collect{ |plugin| plugin.send(method, args) if plugin.respond_to?(method) }.flatten.join
    end
  end
  
  def camelize(lower_case_and_underscored_word)
    lower_case_and_underscored_word.gsub(/\/(.?)/) { "::" + $1.upcase }.gsub(/(^|_)(.)/) { $2.upcase }
  end
end
