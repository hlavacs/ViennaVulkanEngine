# Download Ring Racer's external models and sounds while configuring the examples.
# Fixed commits and archive hashes keep every platform on the same asset versions.
include(FetchContent)
set(vve_racer_asset_dir "${CMAKE_CURRENT_LIST_DIR}/../assets/racer/kenney")

FetchContent_Declare(vve_kenney_racing
    URL https://github.com/KenneyNL/Starter-Kit-Racing/archive/2f2e5f2646dda89cb21d4e8539bab60c6e955dc8.tar.gz
    URL_HASH SHA256=e425583609f6dc6be79361bff3c89d6527acf6a93262ebe5b9a52a9856cbc3c4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(vve_kenney_city
    URL https://github.com/KenneyNL/Starter-Kit-City-Builder/archive/4535092b740b378b700efd9df9e27a631815b84a.tar.gz
    URL_HASH SHA256=110d53ff10fee82fe639b82b600dd3aab05c1ea3b8ef2f16660b69b5da1e8e25
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# These are Godot projects used as asset collections; there is no C++ library to build.
message(STATUS "Preparing Ring Racer's Kenney starter kits")
FetchContent_MakeAvailable(vve_kenney_racing vve_kenney_city)

# Copy complete kits from CMake's cache, preserving texture paths and licenses.
# Running this on every configure also restores a deleted or incomplete asset folder.
file(COPY "${vve_kenney_racing_SOURCE_DIR}/" DESTINATION "${vve_racer_asset_dir}/racing")
file(COPY "${vve_kenney_city_SOURCE_DIR}/" DESTINATION "${vve_racer_asset_dir}/city")
