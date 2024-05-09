require 'rubygems'
require 'rake'
require 'set'
require 'fileutils'
require 'ceedling/file_path_utils'


class FileSystemUtils

  constructor :file_wrapper

  # build up path list from input of one or more strings or arrays of (+/-) paths & globs
  def collect_paths(*paths)
    raw   = []  # all paths and globs
    plus  = Set.new # all paths to expand and add
    minus = Set.new # all paths to remove from plus set

    # assemble all globs and simple paths, reforming our glob notation to ruby globs
    paths.each do |paths_container|
      case (paths_container)
        when String then raw << (FilePathUtils::reform_glob(paths_container))
        when Array  then paths_container.each {|path| raw << (FilePathUtils::reform_glob(path))}
        else raise "Don't know how to handle #{paths_container.class}"
      end
    end

    # iterate through each path and glob
    raw.each do |path|

      dirs = []  # container for only (expanded) paths

      # if a glob, expand it and slurp up all non-file paths
      if path.include?('*')
        # grab base directory only if globs are snug up to final path separator
        if (path =~ /\/\*+$/)
          dirs << FilePathUtils.extract_path(path)
        end

        # grab expanded sub-directory globs
        expanded = @file_wrapper.directory_listing( FilePathUtils.extract_path_no_aggregation_operators(path) )
        expanded.each do |entry|
          dirs << entry if @file_wrapper.directory?(entry)
        end

      # else just grab simple path
      # note: we could just run this through glob expansion but such an
      #       approach doesn't handle a path not yet on disk)
      else
        dirs << FilePathUtils.extract_path_no_aggregation_operators(path)
      end

      # add dirs to the appropriate set based on path aggregation modifier if present
      FilePathUtils.add_path?(path) ? plus.merge(dirs) : minus.merge(dirs)
    end

    return (plus - minus).to_a.uniq
  end


  # given a file list, add to it or remove from it
  def revise_file_list(list, revisions)
    revisions.each do |revision|
      # include or exclude file or glob to file list
      file = FilePathUtils.extract_path_no_aggregation_operators( revision )
      FilePathUtils.add_path?(revision) ? list.include(file) : list.exclude(file)
    end
  end

end
