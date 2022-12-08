
# @private
module Ceedling
  module Version
    { "UNITY" => File.join("unity","src","unity.h"),
      "CMOCK" => File.join("cmock","src","cmock.h"),
      "CEXCEPTION" => File.join("c_exception","lib","CException.h")
    }.each_pair do |name, path|
      # Check for local or global version of vendor directory in order to look up versions
      path1 = File.expand_path( File.join("..","..","vendor",path) )
      path2 = File.expand_path( File.join(File.dirname(__FILE__),"..","..","vendor",path) )
      filename = if (File.exists?(path1))
        path1
      elsif (File.exists?(path2))
        path2
      elsif File.exists?(CEEDLING_VENDOR)
        path3 = File.expand_path( File.join(CEEDLING_VENDOR,path) )
        if (File.exists?(path3))
          path3
        else
          basepath = File.join( CEEDLING_VENDOR, path.split(/\\\//)[0], 'release')
          begin
            [ @ceedling[:file_wrapper].read( File.join(base_path, 'release', 'version.info') ).strip,
              @ceedling[:file_wrapper].read( File.join(base_path, 'release', 'build.info') ).strip ].join('.')
          rescue
            "#{name}"
          end
        end
      else
        module_eval("#{name} = 'unknown'")
        continue
      end

      # Actually look up the versions
      a = [0,0,0]
      begin
        File.readlines(filename).each do |line|
          ["VERSION_MAJOR", "VERSION_MINOR", "VERSION_BUILD"].each_with_index do |field, i|
            m = line.match(/#{name}_#{field}\s+(\d+)/)
            a[i] = m[1] unless (m.nil?)
          end
        end
      rescue
        abort("Can't collect data for vendor component: \"#{filename}\" . \nPlease check your setup.")
      end

      # splat it to return the final value
      eval("#{name} = '#{a.join(".")}'")
    end

    GEM = "0.31.1"
    CEEDLING = GEM
  end
end
