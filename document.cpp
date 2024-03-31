#include "document.h"
#include <tuple>

Document::Document(int id, double relevance, int rating)
	: id(id)
	, relevance(relevance)
	, rating(rating) {}

// перегрузка оператора == для типа Document
bool operator == (const Document &lhs, const Document &rhs)
{
	return std::tie(lhs.id, rhs.rating) == std::tie(lhs.id, rhs.rating) && (abs(lhs.relevance - rhs.relevance) < 1e-6);
}

bool operator != (const Document &lhs, const Document &rhs)
{
	return !(lhs == rhs);
}
// перегрузка оператора < для типа Document
bool operator < (const Document &lhs, const Document &rhs) 
{
	//сравниваем документы по убыванию релевантности и рейтинга
	if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
		return lhs.rating < rhs.rating;
	}
	return lhs.relevance < rhs.relevance;
}

// перегрузка оператора < для типа Document
bool operator > (const Document &lhs, const Document &rhs)
{
	//сравниваем документы по возрастанию  релевантности и рейтинга
	if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
		return lhs.rating > rhs.rating;
	}
	return lhs.relevance > rhs.relevance;
}
//оператор вывода для типа Document
std::ostream& operator<<(std::ostream& out, const Document& document)
{
	out << "{ document_id = " << document.id << ", relevance = " << document.relevance << ", rating = " << document.rating << " }";
	return out;
}