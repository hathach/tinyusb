class ConstructorStruct
  def self.new(*accessors, &block)
    defaults = {:accessors => true, :strict => false}
    
    accessor_names = accessors.dup
    if accessors.last.is_a? Hash
      accessor_names.pop
      user_opts = accessors.last
      user_opts.delete(:accessors)
      defaults.each do |k,v|
        user_opts[k] ||= v
      end
    else
      accessors << defaults
    end
    
    Class.new do
      constructor *accessors

      class_eval(&block) if block

      comparator_code = accessor_names.map { |fname| "self.#{fname} == o.#{fname}" }.join(" && ")
      eval %|
        def ==(o)
          (self.class == o.class) && #{comparator_code}
        end
        def eql?(o)
          (self.class == o.class) && #{comparator_code}
        end
      |
    end
  end
end
