#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <tuple>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <string_view>
#include <execution>
#include <list>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using namespace std::literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EXP = 1e-6;
const int STREAM_MAX = 16;

class SearchServer {
public:
	// Конструктор принимающий контейнер
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	// Конструктор константной строки
	explicit SearchServer(const std::string& stop_words_text) :
	SearchServer(static_cast<std::string_view>(stop_words_text)) {}

	// Конструктор string_view
	SearchServer(std::string_view stop_words_text) :
		SearchServer(SplitIntoWordsView(stop_words_text)) {}

	//функция добавления документов
	void AddDocument(int document_id, std::string_view document, DocumentStatus status,
		const std::vector<int>& ratings);
	
	//по запросу, без фильтраций
	//1
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
		DocumentPredicate document_predicate) const;
	//2
	template <typename Execution,typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(Execution&& policy,
		std::string_view raw_query, DocumentPredicate document_predicate) const;
	//с фильтрацией по статусу и по произвольному предикату
	//3
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
		DocumentStatus status = DocumentStatus::ACTUAL) const {
		return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status; });
	}
	//4
	template <typename Execution>
	std::vector<Document> FindTopDocuments(Execution&& policy,
		std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
		return FindTopDocuments(policy, raw_query,[status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status; });
	}

	int GetDocumentCount() const;

	std::set<int>::iterator begin() const;

	std::set<int>::iterator end() const;

	//метод получения частот слов по id документа, eсли документа не существует, возвратите ссылку на пустой map
	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	// метод удаления документов из поискового сервера
	void RemoveDocument(int document_id);

	//метод удаления документов из поискового сервера, многопоточный
	template<class Execution>
	void RemoveDocument(Execution&& policy, int document_id);

	using ReturnMatch = std::tuple<std::vector<std::string_view>, DocumentStatus>;
	// возвращает пару из вектора слов и структуры DocumentStatus, по запросу query
	//1
	ReturnMatch MatchDocument(std::string_view raw_query,
		int document_id) const;
	//2
	ReturnMatch MatchDocument(const std::execution::sequenced_policy&,
		std::string_view raw_query, int document_id) const;
	//3
	ReturnMatch MatchDocument(const std::execution::parallel_policy& policy,
		std::string_view raw_query, int document_id) const;

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
		std::string document;
	};
	std::set<std::string, std::less<>> stop_words_;        // множество стоп слов
	std::map<std::string_view, std::map<int, double>> word_to_document_; // словарь слов  map<слово, map<id, частота>>   std::less<>
	std::map<int, std::map<std::string_view, double>> document_to_word_; // словарь слов  map<id, map<слово, частота>>
	std::map<int, DocumentData> documents_; // словарь документов <document_id, DocumentData<rating,status,document>>
	std::set<int> document_ids_; // множество id документов на сервере

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	struct QueryVector {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	bool IsStopWord(std::string_view word) const;

	static bool IsValidWord(std::string_view word);

	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	QueryWord ParseQueryWord(std::string_view text) const;
	//для структуры Query
	//Query ParseQuery(std::string_view text) const;
	//для структуры QueryVector
	QueryVector ParseQueryVector(std::string_view text) const;
	// Existence required
	double ComputeWordInverseDocumentFreq(std::string_view word) const;

	// FindAllDocuments - находит и возвращает все документы по запросу, соответствующие предикату
    //1
	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const QueryVector& query,
		DocumentPredicate document_predicate) const;
	//2
	template <typename DocumentPredicate, typename Execution>
	std::vector<Document> FindAllDocuments(Execution&& policy,
		QueryVector& query, DocumentPredicate document_predicate) const;
};

template<class Execution>
void SearchServer::RemoveDocument(Execution&& policy, int document_id)
{
	const auto it_word = document_to_word_.find(document_id);
	if (it_word == document_to_word_.end())
		return;

	// формирование вектора слов документа
	std::vector<std::string_view> words(document_to_word_.at(document_id).size());

	std::transform(
		it_word->second.begin(),
		it_word->second.end(),
		std::back_inserter(words),
		[](auto &word) { return word.first; });


	std::for_each(policy,
		words.begin(), words.end(),
		[&](auto &word) {
		const auto it = word_to_document_.find(word);
		if (it != word_to_document_.end()) {
			it->second.erase(document_id);
		}
	});
	//удаляем в оставшихся словарях
	document_to_word_.erase(document_id);
	documents_.erase(document_id);
	document_ids_.erase(document_id);
}

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
	if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw std::invalid_argument("Some of stop words are invalid");
	}
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
	DocumentPredicate document_predicate) const
{
    auto query = ParseQueryVector(raw_query);
	auto matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);

	sort(matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < EXP) {
			return lhs.rating > rhs.rating;
		}
		else {
			return lhs.relevance > rhs.relevance;
		}
	});

	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}
//2
template <typename Execution, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(Execution&& policy,
	std::string_view raw_query, DocumentPredicate document_predicate) const
{
	auto query = ParseQueryVector(raw_query);
	auto matched_documents = FindAllDocuments(policy, query, document_predicate);

	sort(policy,
		matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < EXP) {
			return lhs.rating > rhs.rating;
		}
		else {
			return lhs.relevance > rhs.relevance;
		}
	});

	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const QueryVector& query, DocumentPredicate document_predicate) const
{

	std::map<int, double> document_to_relevance;
	//плюс слова
	for (std::string_view word : query.plus_words) {

		if (word_to_document_.count((word)) != 0) {
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto[document_id, term_freq] : word_to_document_.at((word))) {
				const auto& document_data = documents_.at(document_id);
				if (document_predicate(document_id, document_data.status, document_data.rating)) {
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}
	}
	//минус слова 
	for (std::string_view word : query.minus_words) {
		if (word_to_document_.count(word) != 0) {
			for (const auto[document_id, _] : word_to_document_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}
	}
	// вектор совпадения документов
	std::vector<Document> matched_documents;
	matched_documents.reserve(document_to_relevance.size());
	for (const auto&[document_id, relevance] : document_to_relevance) {
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}

	return matched_documents;
}

template <typename DocumentPredicate, typename Execution>
std::vector<Document> SearchServer::FindAllDocuments(Execution&& policy,
	QueryVector& query, DocumentPredicate document_predicate) const
{
	ConcurrentMap<int, double> document_to_relevance(STREAM_MAX);

	//сортируем и упорядочеваем к выдаче + слова
	if (!query.plus_words.empty())
	{
		std::sort(policy, query.plus_words.begin(), query.plus_words.end());
		const auto range_end = std::unique(std::execution::par, query.plus_words.begin(), query.plus_words.end());
		query.plus_words.erase(range_end, query.plus_words.end());
	}

	std::for_each(policy,
		query.plus_words.begin(), query.plus_words.end(),
		[this, &document_to_relevance, document_predicate](auto &word) {
		// проходим по всем документам содержащим плюс слова
		if (word_to_document_.count(word) != 0) {
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto[document_id, term_freq] : word_to_document_.at(word)) {
				const auto& document_data = documents_.at(document_id);
				if (document_predicate(document_id, document_data.status, document_data.rating)) {
					document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
				}
			}
		}
	});

	std::map<int, double> document_relevance = document_to_relevance.BuildOrdinaryMap();
	//сортируем и упорядочеваем к выдаче - слова
	if (!query.minus_words.empty())
	{
		std::sort(policy, query.minus_words.begin(), query.minus_words.end());
		const auto range_end = std::unique(policy, query.minus_words.begin(), query.minus_words.end());
		query.minus_words.erase(range_end, query.minus_words.end());
	}

	std::for_each(policy,
		query.minus_words.begin(), query.minus_words.end(),
		[this, &document_relevance](auto &word) {
		if (word_to_document_.count(word) != 0) {
			for (const auto[document_id, _] : word_to_document_.at(word)) {
				document_relevance.erase(document_id);
			}
		}
	});

	std::vector<Document> matched_documents;
	matched_documents.reserve(document_relevance.size());

	for (const auto&[document_id, relevance] : document_relevance) {
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}

	return matched_documents;
}