#pragma once

#include <mdf_common.h>
#include <string>

// sendHttpPost - sends http request and returns positive number on success(http status code) or negative on failure
int sendHttpPost(const std::string& url, const std::string& postData);
