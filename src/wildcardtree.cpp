#include "otpch.h"

#include "wildcardtree.h"

#include <stack>

WildcardTreeNode* WildcardTreeNode::getChild(char ch)
{
	auto it = m_children.find(ch);
	if (it == m_children.end()) {
		return nullptr;
	}
	return &it->second;
}

const WildcardTreeNode* WildcardTreeNode::getChild(char ch) const
{
	auto it = m_children.find(ch);
	if (it == m_children.end()) {
		return nullptr;
	}
	return &it->second;
}

WildcardTreeNode* WildcardTreeNode::addChild(char ch, bool breakpoint)
{
	WildcardTreeNode* child = getChild(ch);
	if (child) {
		if (breakpoint && !child->m_breakpoint) {
			child->m_breakpoint = true;
		}
	} else {
		auto pair = m_children.emplace(std::piecewise_construct,
				std::forward_as_tuple(ch), std::forward_as_tuple(breakpoint));
		child = &pair.first->second;
	}
	return child;
}

void WildcardTreeNode::insert(const std::string& str)
{
	WildcardTreeNode* cur = this;

	size_t length = str.length() - 1;
	for (size_t pos = 0; pos < length; ++pos) {
		cur = cur->addChild(str[pos], false);
	}

	cur->addChild(str[length], true);
}

void WildcardTreeNode::remove(const std::string& str)
{
	WildcardTreeNode* cur = this;

	std::stack<WildcardTreeNode*> path;
	path.push(cur);
	size_t len = str.length();
	for (size_t pos = 0; pos < len; ++pos) {
		cur = cur->getChild(str[pos]);
		if (!cur) {
			return;
		}
		path.push(cur);
	}

	cur->m_breakpoint = false;

	do {
		cur = path.top();
		path.pop();

		if (!cur->m_children.empty() || cur->m_breakpoint || path.empty()) {
			break;
		}

		cur = path.top();

		auto it = cur->m_children.find(str[--len]);
		if (it != cur->m_children.end()) {
			cur->m_children.erase(it);
		}
	} while (true);
}

ReturnValue WildcardTreeNode::findOne(const std::string& query, std::string& result) const
{
	const WildcardTreeNode* cur = this;
	for (char pos : query) {
		cur = cur->getChild(pos);
		if (!cur) {
			return RET_PLAYERWITHTHISNAMEISNOTONLINE;
		}
	}

	result = query;

	do {
		size_t size = cur->m_children.size();
		if (size == 0) {
			return RET_NOERROR;
		} else if (size > 1 || cur->m_breakpoint) {
			return RET_NAMEISTOOAMBIGUOUS;
		}

		auto it = cur->m_children.begin();
		result += it->first;
		cur = &it->second;
	} while (true);
}
