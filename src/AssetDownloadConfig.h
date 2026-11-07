#pragma once

// Download hosts for language and hyphenation packs. Builds set them only from
// CROSSINK_ASSET_BASE_URL (scripts/git_branch.py); empty means this build has no
// download host. Catalog data cannot select a host or redirect.
#ifndef ASSET_DOWNLOAD_BASE_URL
#define ASSET_DOWNLOAD_BASE_URL ""
#endif
#ifndef ASSET_CATALOG_BASE_URL
#define ASSET_CATALOG_BASE_URL ASSET_DOWNLOAD_BASE_URL
#endif
#ifndef ASSET_DOWNLOAD_REDIRECT_BASE_URL
#define ASSET_DOWNLOAD_REDIRECT_BASE_URL ""
#endif

// False without a host: menus that would only lead to "Downloads unavailable"
// stay hidden. A compile-time constant, so the hidden paths leave the image.
constexpr bool ASSET_DOWNLOADS_CONFIGURED = sizeof(ASSET_DOWNLOAD_BASE_URL) > 1 && sizeof(ASSET_CATALOG_BASE_URL) > 1;
