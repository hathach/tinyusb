module DIY #:nodoc:#
  class FactoryDef #:nodoc:
    attr_accessor :name, :target, :class_name, :library
    
    def initialize(opts)
      @name, @target, @library, @auto_require =
        opts[:name], opts[:target], opts[:library], opts[:auto_require]

			@class_name = Infl.camelize(@target)
			@library ||= Infl.underscore(@class_name) if @auto_require
    end
  end
  
	class Context
    def construct_factory(key)
      factory_def = @defs[key]
#      puts "requiring #{factory_def.library}"
      require factory_def.library	if factory_def.library

			big_c = get_class_for_name_with_module_delimeters(factory_def.class_name)

      FactoryFactory.new(big_c)
    end
  end

  class FactoryFactory
    def initialize(clazz)
      @class_to_create = clazz
    end

    def create(*args)
      @class_to_create.new(*args)
    end
  end
end

