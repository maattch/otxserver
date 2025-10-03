#pragma once

#include "const.h"

class WildcardTreeNode final
{
public:
	explicit WildcardTreeNode(bool breakpoint) : m_breakpoint(breakpoint) {}
	WildcardTreeNode(WildcardTreeNode&& other) = default;

	// non-copyable
	WildcardTreeNode(const WildcardTreeNode&) = delete;
	WildcardTreeNode& operator=(const WildcardTreeNode&) = delete;

	WildcardTreeNode* getChild(char ch);
	const WildcardTreeNode* getChild(char ch) const;
	WildcardTreeNode* addChild(char ch, bool breakpoint);

	void insert(const std::string& str);
	void remove(const std::string& str);

	ReturnValue findOne(const std::string& query, std::string& result) const;

private:
	std::map<char, WildcardTreeNode> m_children;
	bool m_breakpoint;
};
