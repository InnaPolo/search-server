#include "process_queries.h"

#include <numeric>
#include <execution>
#include <numeric>

std::vector<std::vector<Document>> ProcessQueries(
	const SearchServer& search_server,
	const std::vector<std::string>& queries) {

	std::vector<std::vector<Document>> result(queries.size());
	std::transform(std::execution::par,
		queries.begin(), queries.end(),
	    result.begin(),
		[&](auto &query) {
		return search_server.FindTopDocuments(query);
	});
	return result;
}

std::vector<Document> ProcessQueriesJoined(
	const SearchServer& search_server,
	const std::vector<std::string>& queries)
{
	std::vector<Document> result;
	int pos = 0;
	for (auto & vec_doc : ProcessQueries(search_server, queries))
	{
		result.insert(result.begin() + pos, vec_doc.begin(), vec_doc.end());
		pos += vec_doc.size();
	}
	return result;
}
