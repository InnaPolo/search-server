#include "test_example_functions.h"
#include "log_duration.h"

using namespace std;

void PrintDocument(const Document& document)
{
	cout << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating << " }"s << endl;
}

void AddDocument(SearchServer &search_server, int document_id, const std::string &document,
	DocumentStatus status, const std::vector<int> &ratings) {
	search_server.AddDocument(document_id, document, status, ratings);
}

void FindTopDocuments(const SearchServer &search_server, const std::string &raw_query)
{
	{
		LOG_DURATION_STREAM("Operation time"s, cout);
		cout << "Search results on query: "s << raw_query << endl;
		auto result = search_server.FindTopDocuments(raw_query);
		for (auto &document : result) {
			PrintDocument(document);
		}
	}
	cout << endl;
}


void MatchDocument(const SearchServer &search_server, const std::string &raw_query, int document_id)
{
	{
		LOG_DURATION_STREAM("Operation time"s, cout);
		cout << "Search results on query: "s << raw_query << endl;
		//return std::tuple<std::vector<std::string>, DocumentStatus>
		const auto[words, status] = search_server.MatchDocument(raw_query, document_id);
	}
	cout << endl;
}
// --------- Начало модульных тестов поисковой системы -----------
template< typename T>
void RunTestImpl(const T& value, const string& value_str) {
	value();
	cerr << value_str << "[+]"s << endl;
}
#define RUN_TEST(func)  RunTestImpl((func), #func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
	const string& func, unsigned line, const string& hint) {
	if (t != u) {
		cout << boolalpha;
		cout << file << "("s << line << "): "s << func << ": "s;
		cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		cout << t << " != "s << u << "."s;
		if (!hint.empty()) {
			cout << " Hint: "s << hint;
		}
		cout << endl;
		abort();
	}
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
	const string& hint) {
	if (!value) {
		cout << file << "("s << line << "): "s << func << ": "s;
		cout << "ASSERT("s << expr_str << ") failed."s;
		if (!hint.empty()) {
			cout << " Hint: "s << hint;
		}
		cout << endl;
		abort();
	}
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------
void TestMachDocument()
{
	SearchServer search_server("and with"s);
	int id = 0;
	for (const string& text : {
			"funny pet and nasty rat"s,
			"funny pet with curly hair"s,
			"funny pet and not very nasty rat"s,
			"pet with rat and rat and rat"s,
			"nasty rat with curly hair"s,
		}
		) {
		search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
	}
	const string query = "curly and funny -not"s;
	{
		LOG_DURATION_STREAM("Operation time MachDocument simple:"s, cout);
		const auto[words, status] = search_server.MatchDocument(query, 1);
		ASSERT_EQUAL(words.size(), 1u);
	}
	{
		LOG_DURATION_STREAM("Operation time MachDocument seq:"s, cout);
		const auto[words, status] = search_server.MatchDocument(execution::seq, query, 2);
		ASSERT_EQUAL(words.size(), 2u);
	}
	{
		LOG_DURATION_STREAM("Operation time MachDocument  par:"s, cout);
		const auto[words, status] = search_server.MatchDocument(execution::par, query, 3);
		ASSERT_EQUAL(words.size(), 0u);
	}
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server{ "cat city"s };
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server{ "in the"s};
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
			"Stop words must be excluded from documents"s);
	}
}
//Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
//void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings)
void TestAddDocument()
{
	SearchServer search_server("и в на"s);
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

	//проверка на содержание стоп слов в документе
	ASSERT(search_server.FindTopDocuments("и в на"s).empty());

	const vector<Document> documents = { { 1, 0.866433, 5 },{ 0, 0.173286, 2 },{ 2, 0.173286,-1 } };
	ASSERT(documents == search_server.FindTopDocuments("пушистый ухоженный кот"s));
	//
	const vector<Document> documentsBUNNED = { { 3,0.231049,9 } };
	ASSERT_HINT(documentsBUNNED == search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED),
		"Output of all documents with status BANNED"s);
	//
	const vector<Document> documents_ids = { { 0,0.173286,2 },{ 2,0.173286,-1 } };
	ASSERT(documents_ids == search_server.FindTopDocuments("пушистый ухоженный кот"s,
		[](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; }));
}
void TestRatingPlus()
{
	SearchServer search_server1{ "и в на"s };
	search_server1.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	{
		const auto found_docs = search_server1.FindTopDocuments("пушистый ухоженный кот"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.rating, 5);
	}
	//
	SearchServer search_server2{ "и в на"s };
	search_server2.AddDocument(2, "пушистый кот пушистый хвост"s, DocumentStatus::IRRELEVANT, { 1,2,8,9,6,10,12 });
	{
		const auto found_docs2 = search_server2.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
		ASSERT_EQUAL(found_docs2.size(), 1u);
		const Document& doc = found_docs2[0];
		ASSERT_EQUAL(doc.rating, 6);
	}
}
void TestRatingMinus()
{
	SearchServer search_server1{ "и в на"s };
	search_server1.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { -7, -2, -7 });
	{
		const auto found_docs = search_server1.FindTopDocuments("пушистый ухоженный кот"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.rating, -5);
	}
	//
	SearchServer search_server2{ "и в на"s };
	search_server2.AddDocument(91, "пушистый кот пушистый хвост"s, DocumentStatus::BANNED, { -1,-2,-8,-9,-6,-10,-12 });
	{
		const auto found_docs2 = search_server2.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
		ASSERT_EQUAL(found_docs2.size(), 1u);
		const Document& doc0 = found_docs2[0];
		ASSERT_EQUAL(doc0.rating, -6);
	}
}

void TestRatingHash()
{
	SearchServer search_server{ "и в на"s };
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	{
		const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.rating, 2);
	}
}

void TestSearchPredicate()
{
	SearchServer search_server{ "и в на"s };
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::BANNED, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::IRRELEVANT, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::REMOVED, { 9 });
	//id
	{
		const vector<Document> documents_ids = { { 3,0.231049,9 }, { 0,0.173286,2 } };
		auto document_current_id = search_server.FindTopDocuments("пушистый ухоженный кот"s,
			[](int document_id, DocumentStatus status, int rating) { return document_id % 3 == 0; });
		ASSERT(documents_ids == document_current_id);

	}
	//status
	{
		const vector<Document> documents_status = { { 1, 0.866433, 5 } };
		auto document_current_status = search_server.FindTopDocuments("пушистый ухоженный кот"s,
			[](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
		ASSERT(documents_status == document_current_status);
	}
	//rating
	{
		const vector<Document> documents_rating = { { 1, 0.866433, 5 } ,{ 3,0.231049,9 },{ 0,0.173286,2 } };
		auto document_current_rating = search_server.FindTopDocuments("пушистый ухоженный кот"s,
			[](int document_id, DocumentStatus status, int rating) { return rating > 0; });
		ASSERT(documents_rating == document_current_rating);
	}
}

void TestSearchAllStatus()
{
	//{ 0,0.173286,2 },{ 1, 0.866433, 5 },{ 2, 0.173286,-1 } ,{ 3,0.231049,9 }
	SearchServer search_server{ "и в на"s };
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::BANNED, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::IRRELEVANT, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::REMOVED, { 9 });
	std::string query = "пушистый ухоженный кот"s;
	//ACTUAL
	const vector<Document> documentsACTUAL = { { 0,0.173286,2 } };
	ASSERT_HINT(documentsACTUAL == search_server.FindTopDocuments(query, DocumentStatus::ACTUAL),
		"Output of all documents with status BANNED"s);
	//BANNED
	const vector<Document> documentsBUNNED = { { 1, 0.866433, 5 } };
	ASSERT_HINT(documentsBUNNED == search_server.FindTopDocuments(query, DocumentStatus::BANNED),
		"Output of all documents with status BANNED"s);
	//IRRELEVANT
	const vector<Document> documentsIRRELEVANT = { { 2, 0.173286,-1 } };
	ASSERT_HINT(documentsIRRELEVANT == search_server.FindTopDocuments(query, DocumentStatus::IRRELEVANT),
		"Output of all documents with status IRRELEVANT"s);
	//REMOVED
	const vector<Document> documentsREMOVED = { { 3,0.231049,9 } };
	ASSERT_HINT(documentsREMOVED == search_server.FindTopDocuments(query, DocumentStatus::REMOVED),
		"Output of all documents with status REMOVED"s);
}

//результаты отсортированны по релевантности
void TestResultsSortRelevance()
{
	SearchServer search_server{ "и в на"s };
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, { 9 });
	std::string query = "пушистый ухоженный кот"s;
	const vector<Document> documents_sort = { { 1, 0.866433, 5 },{ 3,0.231049,9 },{ 0,0.173286,2 },{ 2, 0.173286,-1 } };
	auto document = search_server.FindTopDocuments(query);
	ASSERT(documents_sort == document);
}

void TestResultsSortRelevanceEps()
{
	//{ { 0,0.173286,2 },{ 1, 0.866433, 5 },{ 2, 0.173286,-1 } ,{ 3,0.231049,9 } }
	SearchServer search_server("и в на"s);
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, { 9 });
	{
		const auto  found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		auto value_relevance = 0.173286;
		ASSERT(abs(doc0.relevance - value_relevance) < 1e-6);
	}
}

void TestResultsSortRelevanceEpsError()
{
	//{ { 0,0.173286,2 },{ 1, 0.866433, 5 },{ 2, 0.173286,-1 } ,{ 3,0.231049,9 } }
	SearchServer search_server("и в на"s);
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, { 9 });
	{
		const auto  found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		auto value_relevance = 0.231049; //!< неверное значение
		ASSERT(abs(doc0.relevance - value_relevance) < 1e-6); //!< ошибка допущена специально для прорки корректности работы прграммы
	}
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
	RUN_TEST(TestMachDocument);
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestAddDocument);
	RUN_TEST(TestRatingPlus);
	RUN_TEST(TestRatingMinus);
	RUN_TEST(TestRatingHash);
	RUN_TEST(TestSearchPredicate);
	RUN_TEST(TestSearchAllStatus);
	RUN_TEST(TestResultsSortRelevance);
	RUN_TEST(TestResultsSortRelevanceEps);
	//RUN_TEST(TestResultsSortRelevanceEpsError);
}
// --------- Окончание модульных тестов поисковой системы -----------