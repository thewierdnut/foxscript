

set (big_if )
set (big_inc )
set (inc_files )
set (big_list )

macro(addfile filename)
   string(REGEX REPLACE "[\\./]" "_" varname ${filename})
   #message(${filename} " " ${varname})

   if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
      # if we request a .gz file and it is missing, but we have the
      # original file, then insert a gzip task to compress it first.
      if ("${filename}" MATCHES "\\.gz$")
         string(REGEX REPLACE "\\.gz$" "" orig_filename "${filename}")
         if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${orig_filename})
            message(FATAL "Cannot find ${filename}")
         else()
            add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${filename}
                               COMMAND gzip -9 -f -c ${CMAKE_CURRENT_SOURCE_DIR}/${orig_filename} > ${CMAKE_CURRENT_BINARY_DIR}/${filename}
                               MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${orig_filename}
                               WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
            add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${varname}.h
                               COMMAND xxd -i ${filename} ${CMAKE_CURRENT_BINARY_DIR}/${varname}.h
                               MAIN_DEPENDENCY ${filename}
                               WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
         endif()
      else()
         message(FATAL "Cannot find ${filename}")
      endif()
   else()
      add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${varname}.h
                         COMMAND xxd -i ${filename} ${CMAKE_CURRENT_BINARY_DIR}/${varname}.h
                         MAIN_DEPENDENCY ${filename}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
   endif()


   set (big_if "${big_if}   if (resname == \"${filename}\") return std::pair<const unsigned char*, size_t>(${varname}, ${varname}_len);\n")
   set (big_inc "${big_inc}#include \"${CMAKE_CURRENT_BINARY_DIR}/${varname}.h\"\n")
   set (inc_files ${inc_files} ${CMAKE_CURRENT_BINARY_DIR}/${varname}.h)
   set (big_list "${big_list} \"${filename}\",")
endmacro()


addfile(text_24.png)


file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/res.cxx "
#include \"res.hh\"
#include <map>
#include <fstream>

${big_inc}

static std::map<std::string, std::vector<uint8_t>> g_external_resources;

std::pair<const unsigned char*, size_t> GetResource(const std::string& resname)
{
   // try built-in resources first

${big_if}

   // try loading from file next (todo, reference counting?)
   auto it = g_external_resources.find(resname);
   if (it != g_external_resources.end())
   {
      return std::make_pair(it->second.data(), it->second.size());
   }
   std::ifstream in(\"${CMAKE_SOURCE_DIR}/res/\" + resname, std::ios::binary | std::ios::ate);
   if (in)
   {
      auto& vec = g_external_resources[resname];
      vec.resize(in.tellg());
      in.seekg(0);
      in.read((char*)vec.data(), vec.size());
      return std::make_pair(vec.data(), vec.size());
   }

   return std::make_pair(nullptr, 0);
}

const std::vector<std::string>& GetResourceList()
{
   static std::vector<std::string> resource_list{
      ${big_list}
   };
   return resource_list;
}
")



add_library(res ${CMAKE_CURRENT_BINARY_DIR}/res.cxx ${inc_files})
