CONSTRUCTOR_VERSION = '1.0.4' #:nodoc:#

class Class #:nodoc:#
  def constructor(*attrs, &block)
    call_block = ''
    if block_given?
      @constructor_block = block
      call_block = 'self.instance_eval(&self.class.constructor_block)'
    end
    # Look for embedded options in the listing:
    opts = attrs.find { |a| a.kind_of?(Hash) and attrs.delete(a) } 
    do_acc = opts.nil? ? false : opts[:accessors] == true
    do_reader = opts.nil? ? false : opts[:readers] == true
    require_args = opts.nil? ? true : opts[:strict] != false
    super_args = opts.nil? ? nil : opts[:super]

    # Incorporate superclass's constructor keys, if our superclass
    if superclass.constructor_keys
      similar_keys = superclass.constructor_keys & attrs
      raise "Base class already has keys #{similar_keys.inspect}" unless similar_keys.empty?
      attrs = [attrs,superclass.constructor_keys].flatten
    end
    # Generate ivar assigner code lines
    assigns = ''
    attrs.each do |k|
      assigns += "@#{k.to_s} = args[:#{k.to_s}]\n"
    end 

    # If accessors option is on, declare accessors for the attributes:
    if do_acc
      add_accessors = "attr_accessor " + attrs.reject {|x| superclass.constructor_keys.include?(x.to_sym)}.map {|x| ":#{x.to_s}"}.join(',')
      #add_accessors = "attr_accessor " + attrs.map {|x| ":#{x.to_s}"}.join(',')
      self.class_eval add_accessors
    end

    # If readers option is on, declare readers for the attributes:
    if do_reader
      self.class_eval "attr_reader " + attrs.reject {|x| superclass.constructor_keys.include?(x.to_sym)}.map {|x| ":#{x.to_s}"}.join(',')
    end
    
    # If user supplied super-constructor hints:
    super_call = ''
    if super_args
      list = super_args.map do |a|
        case a
        when String
          %|"#{a}"|
        when Symbol
          %|:#{a}|
        end
      end
      super_call  = %|super(#{list.join(',')})|
    end

    # If strict is on, define the constructor argument validator method,
    # and setup the initializer to invoke the validator method.
    # Otherwise, insert lax code into the initializer.
    validation_code = "return if args.nil?"
    if require_args
      self.class_eval do 
        def _validate_constructor_args(args)
          # First, make sure we've got args of some kind
          unless args and args.keys and args.keys.size > 0 
            raise ConstructorArgumentError.new(self.class.constructor_keys)
          end
          # Scan for missing keys in the argument hash
          a_keys = args.keys
          missing = []
          self.class.constructor_keys.each do |ck|
            unless a_keys.member?(ck)
              missing << ck
            end
            a_keys.delete(ck) # Delete inbound keys as we address them
          end
          if missing.size > 0 || a_keys.size > 0
            raise ConstructorArgumentError.new(missing,a_keys)
          end
        end
      end
      # Setup the code to insert into the initializer:
      validation_code = "_validate_constructor_args args "
    end

    # Generate the initializer code
    self.class_eval %{
      def initialize(args=nil)
        #{super_call}
        #{validation_code}
        #{assigns}
        setup if respond_to?(:setup)
        #{call_block}
      end
    }

    # Remember our constructor keys
    @_ctor_keys = attrs
  end

  # Access the constructor keys for this class
  def constructor_keys; @_ctor_keys ||=[]; end

  def constructor_block #:nodoc:#
    @constructor_block
  end

end

# Fancy validation exception, based on missing and extraneous keys.
class ConstructorArgumentError < RuntimeError #:nodoc:#
  def initialize(missing,rejected=[])
    err_msg = ''
    if missing.size > 0
      err_msg = "Missing constructor args [#{missing.join(',')}]"
    end
    if rejected.size > 0
      # Some inbound keys were not addressed earlier; this means they're unwanted
      if err_msg
        err_msg << "; " # Appending to earlier message about missing items
      else
        err_msg = ''
      end
      # Enumerate the rejected key names
      err_msg << "Rejected constructor args [#{rejected.join(',')}]"
    end
    super err_msg
  end
end
