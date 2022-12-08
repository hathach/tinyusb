require 'diy/factory.rb'
require 'yaml'
require 'set'

module DIY #:nodoc:#
  VERSION = '1.1.2'
  class Context

    class << self
      # Enable / disable automatic requiring of libraries. Default: true
      attr_accessor :auto_require
    end
    @auto_require = true

    # Accepts a Hash defining the object context (usually loaded from objects.yml), and an additional
    # Hash containing objects to inject into the context.
    def initialize(context_hash, extra_inputs={})
      raise "Nil context hash" unless context_hash
      raise "Need a hash" unless context_hash.kind_of?(Hash)
      [ "[]", "keys" ].each do |mname|
        unless extra_inputs.respond_to?(mname) 
          raise "Extra inputs must respond to hash-like [] operator and methods #keys and #each" 
        end
      end

      # store extra inputs
      if extra_inputs.kind_of?(Hash)
        @extra_inputs= {}
        extra_inputs.each { |k,v| @extra_inputs[k.to_s] = v } # smooth out the names
      else
        @extra_inputs = extra_inputs
      end

      collect_object_and_subcontext_defs context_hash

      # init the cache
      @cache = {}
      @cache['this_context'] = self
    end
   

    # Convenience: create a new DIY::Context by loading from a String (or open file handle.)
    def self.from_yaml(io_or_string, extra_inputs={})
      raise "nil input to YAML" unless io_or_string
      Context.new(YAML.load(io_or_string), extra_inputs)
    end

    # Convenience: create a new DIY::Context by loading from the named file.
    def self.from_file(fname, extra_inputs={})
      raise "nil file name" unless fname
      self.from_yaml(File.read(fname), extra_inputs)
    end

    # Return a reference to the object named.  If necessary, the object will 
    # be instantiated on first use.  If the object is non-singleton, a new
    # object will be produced each time.
    def get_object(obj_name)
      key = obj_name.to_s
      obj = @cache[key]
      unless obj
        if extra_inputs_has(key)
          obj = @extra_inputs[key]
        else
          case @defs[key]
          when MethodDef
            obj = construct_method(key)
          when FactoryDef
            obj = construct_factory(key)
            @cache[key] = obj
          else
            obj = construct_object(key)
            @cache[key] = obj if @defs[key].singleton?
          end
        end
      end
      obj
    end
    alias :[] :get_object

    # Inject a named object into the Context.  This must be done before the Context has instantiated the 
    # object in question.
    def set_object(obj_name,obj)
      key = obj_name.to_s
      raise "object '#{key}' already exists in context" if @cache.keys.include?(key)
      @cache[key] = obj
    end
    alias :[]= :set_object

    # Provide a listing of object names
    def keys
      (@defs.keys.to_set + @extra_inputs.keys.to_set).to_a
    end

    # Instantiate and yield the named subcontext
    def within(sub_context_name)
      # Find the subcontext definitaion:
      context_def = @sub_context_defs[sub_context_name.to_s]
      raise "No sub-context named #{sub_context_name}" unless context_def
      # Instantiate a new context using self as parent:
      context = Context.new( context_def, self )

      yield context
    end

    # Returns true if the context contains an object with the given name
    def contains_object(obj_name)
      key = obj_name.to_s
      @defs.keys.member?(key) or extra_inputs_has(key)
    end

    # Every top level object in the Context is instantiated.  This is especially useful for 
    # systems that have "floating observers"... objects that are never directly accessed, who
    # would thus never be instantiated by coincedence.  This does not build any subcontexts 
    # that may exist.
    def build_everything
      @defs.keys.each { |k| self[k] }
    end
    alias :build_all :build_everything
    alias :preinstantiate_singletons :build_everything

    private

    def collect_object_and_subcontext_defs(context_hash)
      @defs = {}
      @sub_context_defs = {}
      get_defs_from context_hash
    end

    def get_defs_from(hash, namespace=nil)
      hash.each do |name,info|
        # we modify the info hash below so it's important to have a new
        # instance to play with
        info = info.dup if info
        
        # see if we are building a factory
        if info and info.has_key?('builds')
          unless info.has_key?('auto_require')
            info['auto_require'] = self.class.auto_require
          end

          if namespace
            info['builds'] = namespace.build_classname(info['builds'])
          end
          @defs[name] = FactoryDef.new({:name => name,
                                       :target => info['builds'],
                                       :library => info['library'],
                                       :auto_require => info['auto_require']})
          next
        end

        name = name.to_s
        case name
        when /^\+/ 
          # subcontext
          @sub_context_defs[name.gsub(/^\+/,'')] = info
          
        when /^using_namespace/
          # namespace: use a module(s) prefix for the classname of contained object defs
          # NOTE: namespacing is NOT scope... it's just a convenient way to setup class names for a group of objects.
          get_defs_from info, parse_namespace(name)
        when /^method\s/
          key_name = name.gsub(/^method\s/, "")
          @defs[key_name] = MethodDef.new(:name => key_name, 
                                      :object => info['object'], 
                                      :method => info['method'],
                                      :attach => info['attach'])
        else 
          # Normal object def
          info ||= {}
          if extra_inputs_has(name)
            raise ConstructionError.new(name, "Object definition conflicts with parent context")
          end
          unless info.has_key?('auto_require')
            info['auto_require'] = self.class.auto_require
          end
          if namespace 
            if info['class']
              info['class'] = namespace.build_classname(info['class'])
            else
              info['class'] = namespace.build_classname(name)
            end
          end
            
          @defs[name] = ObjectDef.new(:name => name, :info => info)

        end
      end
    end

    def construct_method(key)
      method_definition = @defs[key]
      object = get_object(method_definition.object)
      method = object.method(method_definition.method)
      
      unless method_definition.attach.nil?
        instance_var_name = "@__diy_#{method_definition.object}"
        
        method_definition.attach.each do |object_key|
          get_object(object_key).instance_eval do
            instance_variable_set(instance_var_name, object)
            eval %|def #{key}(*args)
              #{instance_var_name}.#{method_definition.method}(*args)
            end|
          end
        end      
      end
      
      return method
    rescue Exception => oops
      build_and_raise_construction_error(key, oops)
    end
    
    def construct_object(key)
      # Find the object definition
      obj_def = @defs[key]
      raise "No object definition for '#{key}'" unless obj_def
      # If object def mentions a library, load it
      require obj_def.library if obj_def.library

      # Resolve all components for the object
      arg_hash = {}
      obj_def.components.each do |name,value|
        case value
        when Lookup
          arg_hash[name.to_sym] = get_object(value.name)
        when StringValue
          arg_hash[name.to_sym] = value.literal_value
        else
          raise "Cannot cope with component definition '#{value.inspect}'"
        end
      end
      # Get a reference to the class for the object
      big_c = get_class_for_name_with_module_delimeters(obj_def.class_name)
      # Make and return the instance
      if obj_def.use_class_directly?
        return big_c
      elsif arg_hash.keys.size > 0
        return big_c.new(arg_hash)
      else
        return big_c.new
      end
    rescue Exception => oops
      build_and_raise_construction_error(key, oops)
    end
    
    def build_and_raise_construction_error(key, oops)
      cerr = ConstructionError.new(key,oops)
      cerr.set_backtrace(oops.backtrace)
      raise cerr
    end
      
    def get_class_for_name_with_module_delimeters(class_name)
      class_name.split(/::/).inject(Object) do |mod,const_name| mod.const_get(const_name) end
    end

    def extra_inputs_has(key)
      if key.nil? or key.strip == ''
        raise ArgumentError.new("Cannot lookup objects with nil keys")
      end
      @extra_inputs.keys.member?(key) or @extra_inputs.keys.member?(key.to_sym)
    end

    def parse_namespace(str)
      Namespace.new(str)
    end
  end
  
  class Namespace #:nodoc:#
    def initialize(str)
      # 'using_namespace Animal Reptile'
      parts = str.split(/\s+/)
      raise "Namespace definitions must begin with 'using_namespace'" unless parts[0] == 'using_namespace'
      parts.shift

      if parts.length > 0 and parts[0] =~ /::/
        parts = parts[0].split(/::/)
      end

      raise NamespaceError, "Namespace needs to indicate a module" if parts.empty?

      @module_nest = parts
    end

    def build_classname(name)
      [ @module_nest, Infl.camelize(name) ].flatten.join("::")
    end
  end

  class Lookup #:nodoc:
    attr_reader :name
    def initialize(obj_name)
      @name = obj_name
    end
  end
  
  class MethodDef #:nodoc:
    attr_accessor :name, :object, :method, :attach
    
    def initialize(opts)
      @name, @object, @method, @attach = opts[:name], opts[:object], opts[:method], opts[:attach]
    end
  end
  
  class ObjectDef #:nodoc:
    attr_accessor :name, :class_name, :library, :components
    def initialize(opts)
      name = opts[:name]
      raise "Can't make an ObjectDef without a name" if name.nil?

      info = opts[:info] || {}
      info = info.clone

      @components = {}

      # Object name
      @name = name

      # Class name
      @class_name = info.delete 'class'
      @class_name ||= info.delete 'type'
      @class_name ||= Infl.camelize(@name)

      # Auto Require
      @auto_require = info.delete 'auto_require'

      # Library
      @library = info.delete 'library'
      @library ||= info.delete 'lib'
      @library ||= Infl.underscore(@class_name) if @auto_require

      # Use Class Directly
      @use_class_directly = info.delete 'use_class_directly'
      
      # Auto-compose
      compose = info.delete 'compose'
      if compose
        case compose
        when Array
          auto_names = compose.map { |x| x.to_s }
        when String
          auto_names = compose.split(',').map { |x| x.to_s.strip }
        when Symbol
          auto_names = [ compose.to_s ]
        else
          raise "Cannot auto compose object #{@name}, bad 'compose' format: #{compose.inspect}"
        end
      end
      auto_names ||= []
      auto_names.each do |cname|
        @components[cname] = Lookup.new(cname)
      end

      # Singleton status
      if info['singleton'].nil?
        @singleton = true
      else
        @singleton = info['singleton']
      end
      info.delete 'singleton'

      # Remaining keys
      info.each do |key,val|
        @components[key.to_s] = Lookup.new(val.to_s)
      end

    end

    def singleton?
      @singleton
    end

    def use_class_directly?
      @use_class_directly == true
    end

  end

  class ConstructionError < RuntimeError #:nodoc:#
    def initialize(object_name, cause=nil)
      object_name = object_name
      cause = cause
      m = "Failed to construct '#{object_name}'"
      if cause 
        m << "\n  ...caused by:\n  >>> #{cause}"
      end
      super m
    end
  end

  class NamespaceError < RuntimeError #:nodoc:#
  end 

  module Infl #:nodoc:#
    # Ganked this from Inflector:
    def self.camelize(lower_case_and_underscored_word) 
      lower_case_and_underscored_word.to_s.gsub(/\/(.?)/) { "::" + $1.upcase }.gsub(/(^|_)(.)/) { $2.upcase }
    end
    # Ganked this from Inflector:
    def self.underscore(camel_cased_word)
      camel_cased_word.to_s.gsub(/::/, '/').gsub(/([A-Z]+)([A-Z])/,'\1_\2').gsub(/([a-z\d])([A-Z])/,'\1_\2').downcase
    end
  end
end
