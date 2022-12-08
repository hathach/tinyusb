# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ==========================================

class CMockConfig
  CMOCK_DEFAULT_OPTIONS =
    {
      :framework                   => :unity,
      :mock_path                   => 'mocks',
      :mock_prefix                 => 'Mock',
      :mock_suffix                 => '',
      :skeleton_path               => '',
      :weak                        => '',
      :subdir                      => nil,
      :plugins                     => [],
      :strippables                 => ['(?:__attribute__\s*\(+.*?\)+)'],
      :attributes                  => %w[__ramfunc __irq __fiq register extern],
      :c_calling_conventions       => %w[__stdcall __cdecl __fastcall],
      :enforce_strict_ordering     => false,
      :fail_on_unexpected_calls    => true,
      :unity_helper_path           => false,
      :treat_as                    => {},
      :treat_as_array              => {},
      :treat_as_void               => [],
      :memcmp_if_unknown           => true,
      :when_no_prototypes          => :warn,           # the options being :ignore, :warn, or :error
      :when_ptr                    => :compare_data,   # the options being :compare_ptr, :compare_data, or :smart
      :verbosity                   => 2,               # the options being 0 errors only, 1 warnings and errors, 2 normal info, 3 verbose
      :treat_externs               => :exclude,        # the options being :include or :exclude
      :treat_inlines               => :exclude,        # the options being :include or :exclude
      :callback_include_count      => true,
      :callback_after_arg_check    => false,
      :includes                    => nil,
      :includes_h_pre_orig_header  => nil,
      :includes_h_post_orig_header => nil,
      :includes_c_pre_header       => nil,
      :includes_c_post_header      => nil,
      :orig_header_include_fmt     => '#include "%s"',
      :array_size_type             => [],
      :array_size_name             => 'size|len',
      :skeleton                    => false,
      :exclude_setjmp_h            => false,

      # Format to look for inline functions.
      # This is a combination of "static" and "inline" keywords ("static inline", "inline static", "inline", "static")
      # There are several possibilities:
      # - sometimes they appear together, sometimes individually,
      # - The keywords can appear before or after the return type (this is a compiler warning but people do weird stuff),
      #   so we check for word boundaries when searching for them
      # - We first remove "static inline" combinations and boil down to single inline or static statements
      :inline_function_patterns    => ['(static\s+inline|inline\s+static)\s*', '(\bstatic\b|\binline\b)\s*'] # Last part (\s*) is just to remove whitespaces (only to prettify the output)
    }.freeze

  def initialize(options = nil)
    case options
    when NilClass then options = CMOCK_DEFAULT_OPTIONS.dup
    when String   then options = CMOCK_DEFAULT_OPTIONS.dup.merge(load_config_file_from_yaml(options))
    when Hash     then options = CMOCK_DEFAULT_OPTIONS.dup.merge(options)
    else raise 'If you specify arguments, it should be a filename or a hash of options'
    end

    # do some quick type verification
    %i[plugins attributes treat_as_void].each do |opt|
      unless options[opt].class == Array
        options[opt] = []
        puts "WARNING: :#{opt} should be an array." unless options[:verbosity] < 1
      end
    end
    %i[includes includes_h_pre_orig_header includes_h_post_orig_header includes_c_pre_header includes_c_post_header].each do |opt|
      unless options[opt].nil? || (options[opt].class == Array)
        options[opt] = []
        puts "WARNING: :#{opt} should be an array." unless options[:verbosity] < 1
      end
    end
    options[:unity_helper_path] ||= options[:unity_helper]
    options[:unity_helper_path] = [options[:unity_helper_path]] if options[:unity_helper_path].is_a? String

    if options[:unity_helper_path]
      require 'pathname'
      includes1 = options[:includes_c_post_header] || []
      includes2 = options[:unity_helper_path].map do |path|
        Pathname(path).relative_path_from(Pathname(options[:mock_path])).to_s
      end
      options[:includes_c_post_header] = (includes1 + includes2).uniq
    end

    options[:plugins].compact!
    options[:plugins].map!(&:to_sym)
    @options = options

    treat_as_map = standard_treat_as_map # .clone
    treat_as_map.merge!(@options[:treat_as])
    @options[:treat_as] = treat_as_map

    @options.each_key do |key|
      unless methods.include?(key)
        eval("def #{key}() return @options[:#{key}] end")
      end
    end
  end

  def load_config_file_from_yaml(yaml_filename)
    self.class.load_config_file_from_yaml yaml_filename
  end

  def self.load_config_file_from_yaml(yaml_filename)
    require 'yaml'
    require 'fileutils'
    YAML.load_file(yaml_filename)[:cmock]
  end

  def path(new_path)
    @src_path = new_path
  end

  def load_unity_helper
    return nil unless @options[:unity_helper_path]

    @options[:unity_helper_path].inject('') do |unity_helper, filename|
      unity_helper + "\n" + File.new(filename).read
    end
  end

  def standard_treat_as_map
    {
      'int'             => 'INT',
      'char'            => 'INT8',
      'short'           => 'INT16',
      'long'            => 'INT',
      'int8'            => 'INT8',
      'int16'           => 'INT16',
      'int32'           => 'INT',
      'int8_t'          => 'INT8',
      'int16_t'         => 'INT16',
      'int32_t'         => 'INT',
      'INT8_T'          => 'INT8',
      'INT16_T'         => 'INT16',
      'INT32_T'         => 'INT',
      'bool'            => 'INT',
      'bool_t'          => 'INT',
      'BOOL'            => 'INT',
      'BOOL_T'          => 'INT',
      'unsigned int'    => 'HEX32',
      'unsigned long'   => 'HEX32',
      'uint32'          => 'HEX32',
      'uint32_t'        => 'HEX32',
      'UINT32'          => 'HEX32',
      'UINT32_T'        => 'HEX32',
      'void*'           => 'HEX8_ARRAY',
      'void const*'     => 'HEX8_ARRAY',
      'const void*'     => 'HEX8_ARRAY',
      'unsigned short'  => 'HEX16',
      'uint16'          => 'HEX16',
      'uint16_t'        => 'HEX16',
      'UINT16'          => 'HEX16',
      'UINT16_T'        => 'HEX16',
      'unsigned char'   => 'HEX8',
      'uint8'           => 'HEX8',
      'uint8_t'         => 'HEX8',
      'UINT8'           => 'HEX8',
      'UINT8_T'         => 'HEX8',
      'char*'           => 'STRING',
      'char const*'     => 'STRING',
      'const char*'     => 'STRING',
      'pCHAR'           => 'STRING',
      'cstring'         => 'STRING',
      'CSTRING'         => 'STRING',
      'float'           => 'FLOAT',
      'double'          => 'FLOAT'
    }
  end
end
