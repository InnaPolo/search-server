#pragma once

#include "search_server.h"

#include <string>
#include <vector>

void PrintDocument(const Document& document);

void AddDocument(SearchServer &search_server, int document_id, const std::string &document,
	DocumentStatus status, const std::vector<int> &ratings);

void FindTopDocuments(const SearchServer &search_server, const std::string &raw_query);

void MatchDocument(const SearchServer &search_server, const std::string &raw_query, int document_id);

//тесты
void TestMachDocument();
void TestExcludeStopWordsFromAddedDocumentContent();
void TestAddDocument();
void TestRatingPlus();
void TestRatingMinus();
void TestRatingHash();
void TestSearchPredicate();
void TestSearchAllStatus();
void TestResultsSortRelevance();
void TestResultsSortRelevanceEps();
void TestResultsSortRelevanceEpsError();
//главный тест
void TestSearchServer();
