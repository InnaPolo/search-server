#pragma once
#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(
	const SearchServer& search_server,
	const std::vector<std::string>& queries);

//распараллеливать обработку нескольких запросов к поисковой системе,
//но возвращать набор документов в плоском виде.Объявление функции 
 std::vector<Document> ProcessQueriesJoined(
	const SearchServer& search_server,
	const std::vector<std::string>& queries);
