#include "request_queue.h"

int RequestQueue::GetNoResultRequests() const
{
	return no_results_requests_;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status)
{
	const auto result = search_server_.FindTopDocuments(raw_query, status);
	AddResult(result.size());
	return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) 
{
	const auto result = search_server_.FindTopDocuments(raw_query);
	AddResult(result.size());
	return result;
}

void RequestQueue::AddResult(int results_num)
{
		// новый запрос - новая секунда
		++current_time_;
		// удаляем все результаты поиска, которые устарели
		while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
			if (0 == requests_.front().results) {
				--no_results_requests_;
			}
			requests_.pop_front();
		}
		// сохраняем новый результат поиска
		requests_.push_back({ current_time_, results_num });
		if (0 == results_num) {
			++no_results_requests_;
		}
}