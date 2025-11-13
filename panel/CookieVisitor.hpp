/******************************************************************************
 Copyright (C) 2025 by Patrick Heyer <PatTheMav@users.noreply.github.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#pragma once

#include <include/cef_cookie.h>

#include <string>
#include <functional>

using CookieVisitorCallback = std::function<void(bool)>;

class CookieVisitor : public CefCookieVisitor {
private:
    CookieVisitorCallback callback_;
    
    std::string target_;
    bool hasMatchingCookie_ = false;

public:
    CookieVisitor(const std::string target, CookieVisitorCallback callback) : target_(target), callback_(callback) {}
    
    virtual ~CookieVisitor() override;
    
    virtual bool Visit(const CefCookie &cookie, int, int, bool &) override;
    
    IMPLEMENT_REFCOUNTING(CookieVisitor);
};

