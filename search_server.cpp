#include "search_server.h"
#include "string_processing.h"

#include <numeric>
#include <iostream>
#include <execution>

using namespace std;

void SearchServer::AddDocument(int document_id, std::string_view document,
	DocumentStatus status, const std::vector<int>& ratings) {

	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw invalid_argument("Invalid document_id"s);
	}

	//записали документ, как строку в DocumentData
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, string(document) });
	document_ids_.insert(document_id);

	//передали документ
	const auto words = SplitIntoWordsNoStop( static_cast<std::string_view>(documents_.at(document_id).document));
	const double inv_word_count = 1.0 / words.size();

	for (std::string_view word : words)
	{
		word_to_document_[word][document_id] += inv_word_count;
		document_to_word_[document_id][word_to_document_.find(word)->first] += inv_word_count;
	}
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

std::set<int>::iterator SearchServer::begin() const {
	return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() const {
	return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
	//O(log N)O(logN)
	const static std::map<std::string_view, double> empty_results = {};
	if (document_ids_.count(document_id) == 0) {
		return empty_results;
	}
	return document_to_word_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
	RemoveDocument(std::execution::seq, document_id);
}

SearchServer::ReturnMatch SearchServer::MatchDocument(std::string_view raw_query,
	int document_id) const
{
	if ((document_id < 0) || !documents_.count(document_id)) {
		throw invalid_argument("Invalid document_id"s);
	}

	auto query = ParseQueryVector(raw_query);
	std::vector<std::string_view> matched_words;

	//обработка минус слов
	//если в документе есть минус слово возвращаем пустой результат
	for (auto& word : query.minus_words) {
		if (word_to_document_.count(word) == 0) {
			continue;
		}
		if (word_to_document_.at(word).count(document_id)) {
			matched_words.clear();
			return { matched_words, documents_.at(document_id).status };
		}
	}
	//проверка на плюс слова
	matched_words.reserve(query.plus_words.size());

	for (auto& word : query.plus_words) {
		if (word_to_document_.count(word) == 0) {
			continue;
		}
		if (word_to_document_.at(word).count(document_id)) {
			matched_words.push_back(word);
		}
	}
	//сортируем и упорядочеваем к выдаче
	std::sort(execution::par, matched_words.begin(), matched_words.end());
	auto range_end = std::unique(execution::par, matched_words.begin(), matched_words.end());
	matched_words.erase(range_end, matched_words.end());

	return { matched_words, documents_.at(document_id).status };
}

SearchServer::ReturnMatch SearchServer::MatchDocument(const std::execution::sequenced_policy&,
	std::string_view raw_query, int document_id) const
{
	return MatchDocument(raw_query, document_id);
}

SearchServer::ReturnMatch  SearchServer::MatchDocument(const std::execution::parallel_policy&,
	std::string_view raw_query, int document_id) const
{
	if ((document_id < 0) || !documents_.count(document_id)) {
		throw invalid_argument("Invalid document_id"s);
	}

	const auto query = ParseQueryVector(raw_query);
	std::vector<std::string_view> matched_words{};

	//обработка минус слов
	//если в документе есть минус слово возвращаем пустой результат
	if (std::any_of(execution::par,
		query.minus_words.begin(),
		query.minus_words.end(),
		[&](const auto &word)
	{ return document_to_word_.at(document_id).count(word) != 0; 	}))
		return { matched_words, documents_.at(document_id).status };
	//обработка плюс слов

	matched_words.reserve(query.plus_words.size());
	std::copy_if(execution::par,
		query.plus_words.begin(),
		query.plus_words.end(),
		std::back_inserter(matched_words),
		[&](const auto &word) {
		return document_to_word_.at(document_id).count(word) != 0; });

	//сортируем и упорядочеваем к выдаче
	std::sort(execution::par, matched_words.begin(), matched_words.end());
	auto range_end = std::unique(execution::par, matched_words.begin(), matched_words.end());
	matched_words.erase(range_end, matched_words.end());

	return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(std::string_view word) const
{
	return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word)
{
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
	});
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const
{
	std::vector<std::string_view> words;
	for (std::string_view word: SplitIntoWordsView(text))
	{
		if (!IsValidWord(word)) {
			throw invalid_argument("Word "s + string(word) + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
	if (ratings.empty()) {
		return 0;
	}
	return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
{
	if (text.empty())
		throw invalid_argument("Query word is empty!");

	bool is_minus = false;
	// Word shouldn't be empty
	if (text[0] == '-') {
		is_minus = true;
		text = text.substr(1);
	}
	//Отсутствие текста после символа минус в поисковом запросе и наличие более чем одного минуса перед словами
	if (text.empty() || !IsValidWord(text)) {
		throw invalid_argument("Query has incorrect symbols in " + string(text));
	}
	if (is_minus && text[0] == '-') {
		throw invalid_argument("Query has incorrect minus-words."s);
	}
	return { text, is_minus, IsStopWord(static_cast<string>(text)) };
}

SearchServer::QueryVector SearchServer::ParseQueryVector(std::string_view text) const
{
	QueryVector result;
	for (auto& word : SplitIntoWordsView(text))
	{
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			(query_word.is_minus) ? result.minus_words.push_back(query_word.data) : result.plus_words.push_back(query_word.data);
		}
	}
	return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const
{
	return log(GetDocumentCount() * 1.0 / word_to_document_.at(std::string(word)).size());
}
