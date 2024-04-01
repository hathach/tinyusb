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
      object_name = 'CMockGeneratorPlugin' + camelize(plugin_name)
      self.class.mutex.synchronize { load_plugin(plugin_name, object_name, config, utils) }
    end
    @plugins.sort! { |a, b| a.priority <=> b.priority }
  end

  def run(method, args = nil)
    if args.nil?
      @plugins.collect { |plugin| plugin.send(method) if plugin.respond_to?(method) }.flatten.join
    else
      @plugins.collect { |plugin| plugin.send(method, args) if plugin.respond_to?(method) }.flatten.join
    end
  end

  def camelize(lower_case_and_underscored_word)
    lower_case_and_underscored_word.gsub(/\/(.?)/) { '::' + Regexp.last_match(1).upcase }.gsub(/(^|_)(.)/) { Regexp.last_match(2).upcase }
  end

  def self.mutex
    @mutex ||= Mutex.new
  end

  private

  def load_plugin(plugin_name, object_name, config, utils)
    unless Object.const_defined? object_name
      file_name = "#{__dir__}/cmock_generator_plugin_#{plugin_name.downcase}.rb"
      require file_name
    end
    class_name = Object.const_get(object_name)
    @plugins << class_name.new(config, utils)
  rescue StandardError
    file_name = "#{__dir__}/cmock_generator_plugin_#{plugin_name.downcase}.rb"
    raise "ERROR: CMock unable to load plugin '#{plugin_name}' '#{object_name}' #{file_name}"
  end
end
