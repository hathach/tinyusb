
# @private
module Ceedling
  module Version
    # Check for local or global version of vendor directory in order to look up versions
    { 
      "CEXCEPTION" => File.join("vendor","c_exception","lib","CException.h"),
      "CMOCK"      => File.join("vendor","cmock","src","cmock.h"),
      "UNITY"      => File.join("vendor","unity","src","unity.h"),
    }.each_pair do |name, path|
      filename = if (File.exist?(File.join("..","..",path)))
        File.join("..","..",path)
      elsif (File.exist?(File.join(File.dirname(__FILE__),"..","..",path)))
        File.join(File.dirname(__FILE__),"..","..",path)
      else
        eval "#{name} = 'unknown'"
        continue
      end

      # Actually look up the versions
      a = [0,0,0]
      File.readlines(filename) do |line|
        ["VERSION_MAJOR", "VERSION_MINOR", "VERSION_BUILD"].each_with_index do |field, i|
          m = line.match(/#{name}_#{field}\s+(\d+)/)
          a[i] = m[1] unless (m.nil?)
        end
      end

      # Make a constant from each, so that we can use it elsewhere
      eval "#{name} = '#{a.join(".")}'"
    end

    GEM = "0.29.0"
    CEEDLING = GEM
  end
end
