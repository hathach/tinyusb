require 'ceedling/plugin'
require 'ceedling/constants'

SUBPROJECTS_ROOT_NAME         = 'subprojects'
SUBPROJECTS_TASK_ROOT         = SUBPROJECTS_ROOT_NAME + ':'
SUBPROJECTS_SYM               = SUBPROJECTS_ROOT_NAME.to_sym

class Subprojects < Plugin

  def setup
    @plugin_root = File.expand_path(File.join(File.dirname(__FILE__), '..'))

    # Add to the test paths
    SUBPROJECTS_PATHS.each do |subproj|
      subproj[:source].each do |path|
        COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR << path
      end
      subproj[:include].each do |path|
        COLLECTION_PATHS_TEST_SUPPORT_SOURCE_INCLUDE_VENDOR << path
      end
    end

    #gather information about the subprojects
    @subprojects = {}
    @subproject_lookup_by_path = {}
    SUBPROJECTS_PATHS.each do |subproj|
      @subprojects[ subproj[:name] ] = subproj.clone
      @subprojects[ subproj[:name] ][:c] = []
      @subprojects[ subproj[:name] ][:asm] = []
      subproj[:source].each do |path|
        search_path = "#{path[-1].match(/\\|\//) ? path : "#{path}/"}*#{EXTENSION_SOURCE}"
        @subprojects[ subproj[:name] ][:c] += Dir[search_path]
        if (EXTENSION_ASSEMBLY && !EXTENSION_ASSEMBLY.empty?)
          search_path = "#{path[-1].match(/\\|\//) ? path : "#{path}/"}*#{EXTENSION_ASSEMBLY}"
          @subprojects[ subproj[:name] ][:asm] += Dir[search_path]
        end
      end
      @subproject_lookup_by_path[ subproj[:build_root] ] = subproj[:name]
    end
  end

  def find_my_project( c_file, file_type = :c )
    @subprojects.each_pair do |subprojname, subproj|
      return subprojname if (subproj[file_type].include?(c_file))
    end
  end

  def find_my_paths( c_file, file_type = :c )
    @subprojects.each_pair do |subprojname, subproj|
      return (subproj[:source] + (subproj[:include] || [])) if (subproj[file_type].include?(c_file))
    end
    return []
  end

  def find_my_defines( c_file, file_type = :c )
    @subprojects.each_pair do |subprojname, subproj|
      return (subproj[:defines] || []) if (subproj[file_type].include?(c_file))
    end
    return []
  end

  def list_all_object_files_for_subproject( lib_name )
    subproj = File.basename(lib_name, EXTENSION_SUBPROJECTS)
    objpath = "#{@subprojects[subproj][:build_root]}/out/c"
    bbb = @subprojects[subproj][:c].map{|f| "#{objpath}/#{File.basename(f,EXTENSION_SOURCE)}#{EXTENSION_OBJECT}" }
    bbb
  end

  def find_library_source_file_for_object( obj_name )
    cname = "#{File.basename(obj_name, EXTENSION_OBJECT)}#{EXTENSION_SOURCE}"
    dname = File.dirname(obj_name)[0..-7]
    pname = @subproject_lookup_by_path[dname]
    return @ceedling[:file_finder].find_file_from_list(cname, @subprojects[pname][:c], :error)
  end

  def find_library_assembly_file_for_object( obj_name )
    cname = "#{File.basename(obj_name, EXTENSION_OBJECT)}#{EXTENSION_ASEMBLY}"
    dname = File.dirname(obj_name)[0..-7]
    pname = @subproject_lookup_by_path[dname]
    return @ceedling[:file_finder].find_file_from_list(cname, @subprojects[pname][:asm], :error)
  end

  def replace_constant(constant, new_value)
    Object.send(:remove_const, constant.to_sym) if (Object.const_defined? constant)
    Object.const_set(constant, new_value)
  end
 
end

# end blocks always executed following rake run
END {
}
