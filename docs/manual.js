(function() {
  var header = document.getElementById("header_wrap");

  function updateSidebarOffset() {
    if (!header) {
      return;
    }
    document.documentElement.style.setProperty("--manual-sidebar-offset", (header.offsetHeight + 20) + "px");
  }

  updateSidebarOffset();
  window.addEventListener("resize", updateSidebarOffset);

  var sidebarLinks = Array.prototype.slice.call(document.querySelectorAll(".manual-sidebar a[href^='#']"));
  var searchInput = document.getElementById("manual-search-input");
  var searchResults = document.getElementById("manual-search-results");
  var searchEmpty = document.getElementById("manual-search-empty");
  var manualBody = document.querySelector(".manual-body");
  var searchMinimap = document.getElementById("manual-search-minimap");
  var searchMinimapViewport = document.getElementById("manual-search-minimap-viewport");
  var activeSearchId = null;
  var currentSearchQuery = "";
  var currentSearchMatches = [];
  var searchMinimapMarkers = [];

  if (!sidebarLinks.length) {
    return;
  }

  var linkMap = new Map();
  var searchIndex = [];

  function headingLevel(node) {
    if (!node || !node.tagName) {
      return null;
    }
    var match = node.tagName.match(/^H([2-6])$/i);
    return match ? parseInt(match[1], 10) : null;
  }

  function scopedSearchNodes(target) {
    if (!target) {
      return [];
    }

    var level = headingLevel(target);
    if (!level) {
      return [target];
    }

    var nodes = [target];
    var node = target.nextElementSibling;

    while (node) {
      var nodeLevel = headingLevel(node);
      if (nodeLevel && nodeLevel <= level) {
        break;
      }
      nodes.push(node);
      node = node.nextElementSibling;
    }

    return nodes;
  }

  function scopedSearchText(target) {
    return scopedSearchNodes(target)
      .map(function(node) {
        return node.textContent.replace(/\s+/g, " ").trim();
      })
      .join(" ")
      .trim();
  }

  function normalizeText(value) {
    return (value || "").replace(/\s+/g, " ").trim();
  }

  function directSidebarAnchor(li) {
    if (!li) {
      return null;
    }
    for (var i = 0; i < li.children.length; i++) {
      var child = li.children[i];
      if (child.tagName === "A" && child.getAttribute("href") && child.getAttribute("href").charAt(0) === "#") {
        return child;
      }
    }
    return null;
  }

  function sidebarLinkPath(link) {
    var path = [normalizeText(link.textContent)];
    var currentLi = link.parentElement && link.parentElement.tagName === "LI" ? link.parentElement : null;

    while (currentLi) {
      var parentList = currentLi.parentElement;
      if (!parentList) {
        break;
      }
      var parentLi = parentList.parentElement && parentList.parentElement.tagName === "LI" ? parentList.parentElement : null;
      if (!parentLi) {
        break;
      }
      var parentLink = directSidebarAnchor(parentLi);
      if (!parentLink) {
        break;
      }
      path.unshift(normalizeText(parentLink.textContent));
      currentLi = parentLi;
    }

    return path;
  }

  sidebarLinks.forEach(function(link) {
    var id = link.getAttribute("href").slice(1);
    var target = document.getElementById(id);
    if (target) {
      linkMap.set(target, link);
      var title = normalizeText(link.textContent);
      var path = sidebarLinkPath(link);
      var pathLabel = path.join(" -> ");
      var context = scopedSearchText(target);
      var entry = {
        id: id,
        title: title,
        path: path,
        pathLabel: pathLabel,
        text: (title + " " + pathLabel + " " + context).toLowerCase()
      };
      searchIndex.push(entry);
    }
  });

  function clearSearchResults() {
    activeSearchId = null;
    currentSearchQuery = "";
    currentSearchMatches = [];
    if (searchResults) {
      searchResults.innerHTML = "";
      searchResults.hidden = true;
    }
    if (searchEmpty) {
      searchEmpty.hidden = true;
    }
    clearSearchHighlights();
    clearSearchMinimap();
  }

  function clearSearchHighlights() {
    Array.prototype.slice.call(document.querySelectorAll(".manual-search-hit")).forEach(function(element) {
      element.classList.remove("manual-search-hit");
    });
    clearSearchMinimapHits();
  }

  function escapeRegExp(value) {
    return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  }

  function clamp01(value) {
    return Math.max(0, Math.min(1, value));
  }

  function clearSearchMinimap() {
    if (!searchMinimap) {
      return;
    }
    Array.prototype.slice.call(searchMinimap.querySelectorAll(".manual-search-minimap-marker")).forEach(function(marker) {
      marker.remove();
    });
    searchMinimapMarkers = [];
    searchMinimap.hidden = true;
    updateSearchMinimapViewport();
  }

  function clearSearchMinimapHits() {
    searchMinimapMarkers.forEach(function(entry) {
      entry.element.classList.remove("is-hit");
    });
  }

  function setSearchMinimapActiveBlock(block) {
    searchMinimapMarkers.forEach(function(entry) {
      entry.element.classList.toggle("is-active", !!block && entry.block === block);
    });
  }

  function setSearchMinimapHitBlocks(blocks) {
    var hits = new Set(blocks || []);
    searchMinimapMarkers.forEach(function(entry) {
      entry.element.classList.toggle("is-hit", hits.has(entry.block));
    });
  }

  function searchMinimapBlocksForMatch(matchId) {
    return searchMinimapMarkers
      .filter(function(entry) {
        return entry.matchId === matchId;
      })
      .map(function(entry) {
        return entry.block;
      });
  }

  function updateSearchMinimapViewport() {
    if (!searchMinimap || !searchMinimapViewport || searchMinimap.hidden || !manualBody) {
      return;
    }

    var contentTop = window.scrollY + manualBody.getBoundingClientRect().top;
    var contentHeight = Math.max(1, manualBody.scrollHeight);

    var viewStart = clamp01((window.scrollY - contentTop) / contentHeight);
    var viewEnd = clamp01((window.scrollY + window.innerHeight - contentTop) / contentHeight);
    if (viewEnd <= viewStart) {
      viewEnd = Math.min(1, viewStart + 0.02);
    }

    searchMinimapViewport.style.top = (viewStart * 100) + "%";
    searchMinimapViewport.style.height = ((viewEnd - viewStart) * 100) + "%";
  }

  function closestSearchHighlightBlock(node, root) {
    var element = node && node.nodeType === Node.ELEMENT_NODE ? node : node && node.parentElement;
    while (element && element !== root) {
      if (element.matches && element.matches("p, li, td, th, pre, h3, h4, h5, h6")) {
        return element;
      }
      element = element.parentElement;
    }
    return root && root.matches && root.matches("p, li, td, th, pre, h3, h4, h5, h6") ? root : null;
  }

  function findMatchingBlocksInRoots(roots, query) {
    if (!query) {
      return [];
    }

    var terms = query.toLowerCase().trim().split(/\s+/).filter(Boolean);
    if (!terms.length) {
      return [];
    }

    var blocks = [];
    var seenBlocks = new Set();

    roots.forEach(function(root) {
      var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
        acceptNode: function(node) {
          if (!node.nodeValue || !node.nodeValue.trim()) {
            return NodeFilter.FILTER_REJECT;
          }
          var parent = node.parentNode;
          if (!parent || /^(SCRIPT|STYLE|MARK)$/i.test(parent.nodeName)) {
            return NodeFilter.FILTER_REJECT;
          }
          var nodeText = node.nodeValue.toLowerCase();
          return terms.some(function(term) {
            return nodeText.indexOf(term) !== -1;
          }) ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT;
        }
      });

      while (walker.nextNode()) {
        var block = closestSearchHighlightBlock(walker.currentNode, root);
        if (block && !seenBlocks.has(block)) {
          seenBlocks.add(block);
          blocks.push(block);
        }
      }
    });

    return blocks.filter(function(block) {
      var blockText = (block.textContent || "").toLowerCase();
      return terms.every(function(term) {
        return blockText.indexOf(term) !== -1;
      });
    });
  }

  function blinkAndKeepSearchHighlight(block) {
    if (!block) {
      return;
    }
    block.classList.remove("manual-search-hit");
    void block.offsetWidth;
    block.classList.add("manual-search-hit");
  }

  function renderSearchMinimap(matches, query) {
    if (!searchMinimap || !manualBody) {
      return;
    }

    var entries = [];
    var seenBlocks = new Set();
    clearSearchMinimap();

    matches.forEach(function(match) {
      var target = document.getElementById(match.id);
      if (!target) {
        return;
      }
      var blocks = findMatchingBlocksInRoots(scopedSearchNodes(target), query);
      blocks.forEach(function(block) {
        if (seenBlocks.has(block)) {
          return;
        }
        seenBlocks.add(block);
        entries.push({
          block: block,
          matchId: match.id,
          pathLabel: match.pathLabel || match.title
        });
      });
    });

    if (!entries.length) {
      return;
    }

    searchMinimap.hidden = false;

    var contentTop = window.scrollY + manualBody.getBoundingClientRect().top;
    var contentHeight = Math.max(1, manualBody.scrollHeight);

    entries.sort(function(a, b) {
      var topA = window.scrollY + a.block.getBoundingClientRect().top;
      var topB = window.scrollY + b.block.getBoundingClientRect().top;
      return topA - topB;
    });

    entries.forEach(function(entry) {
      var block = entry.block;
      var blockTop = window.scrollY + block.getBoundingClientRect().top - contentTop;
      var ratio = clamp01(blockTop / contentHeight);
      var marker = document.createElement("button");
      marker.type = "button";
      marker.className = "manual-search-minimap-marker";
      marker.style.top = (ratio * 100) + "%";
      marker.title = entry.pathLabel;
      marker.setAttribute("aria-label", "Jump to search result: " + entry.pathLabel);
      marker.addEventListener("click", function(event) {
        event.preventDefault();
        if (entry.matchId) {
          activeSearchId = entry.matchId;
          renderSearchResults(currentSearchMatches, currentSearchQuery);
        }
        var target = entry.matchId ? document.getElementById(entry.matchId) : null;
        var highlightResult = (target && currentSearchQuery) ? highlightSearchTerms(target, currentSearchQuery) : null;
        var scopedBlocks = highlightResult ? highlightResult.blocks : searchMinimapBlocksForMatch(entry.matchId);
        clearSearchHighlights();
        scopedBlocks.forEach(function(hitBlock) {
          hitBlock.classList.add("manual-search-hit");
        });
        blinkAndKeepSearchHighlight(block);
        setSearchMinimapHitBlocks(scopedBlocks.length ? scopedBlocks : [block]);
        setSearchMinimapActiveBlock(block);
        block.scrollIntoView({ behavior: "smooth", block: "center" });
        if (entry.matchId && window.location.hash !== "#" + entry.matchId) {
          history.replaceState(null, "", "#" + entry.matchId);
        }
      });
      searchMinimap.appendChild(marker);
      searchMinimapMarkers.push({
        element: marker,
        block: block,
        matchId: entry.matchId
      });
    });

    var allBlocks = entries.map(function(entry) { return entry.block; });
    if (activeSearchId) {
      var activeBlocks = searchMinimapBlocksForMatch(activeSearchId);
      setSearchMinimapHitBlocks(activeBlocks.length ? activeBlocks : allBlocks);
      setSearchMinimapActiveBlock(activeBlocks.length ? activeBlocks[0] : null);
    } else {
      setSearchMinimapHitBlocks(allBlocks);
    }

    updateSearchMinimapViewport();
  }

  function highlightSearchTerms(target, query) {
    if (!target || !query) {
      return null;
    }

    clearSearchHighlights();

    var blocks = findMatchingBlocksInRoots(scopedSearchNodes(target), query);
    blocks.forEach(function(block) {
      block.classList.add("manual-search-hit");
    });

    return {
      first: blocks.length ? blocks[0] : null,
      blocks: blocks
    };
  }

  function renderSearchResults(matches, query) {
    if (!searchResults) {
      return;
    }

    currentSearchMatches = matches.slice();
    searchResults.innerHTML = "";
    matches.slice(0, 12).forEach(function(match) {
      var item = document.createElement("li");
      var link = document.createElement("a");
      link.href = "#" + match.id;
      link.textContent = match.pathLabel || match.title;
      if (match.id === activeSearchId) {
        link.classList.add("is-active");
      }
      link.addEventListener("click", function(event) {
        event.preventDefault();
        activeSearchId = match.id;
        renderSearchResults(currentSearchMatches, currentSearchQuery);
        var target = document.getElementById(match.id);
        var highlightTarget = null;
        var scopedBlocks = [];
        if (currentSearchQuery) {
          var scope = target || null;
          var highlightResult = highlightSearchTerms(scope, currentSearchQuery);
          highlightTarget = highlightResult ? highlightResult.first : null;
          scopedBlocks = highlightResult ? highlightResult.blocks : [];
          if (!scopedBlocks.length) {
            scopedBlocks = searchMinimapBlocksForMatch(match.id);
          }
          setSearchMinimapHitBlocks(scopedBlocks);
          setSearchMinimapActiveBlock(highlightTarget || (scopedBlocks.length ? scopedBlocks[0] : null));
        }
        if (highlightTarget || target) {
          (highlightTarget || target).scrollIntoView({ behavior: "smooth", block: "center" });
        }
        if (target && window.location.hash !== "#" + match.id) {
          history.replaceState(null, "", "#" + match.id);
        }
      });
      item.appendChild(link);
      searchResults.appendChild(item);
    });

    searchResults.hidden = matches.length === 0;
    if (searchEmpty) {
      searchEmpty.hidden = matches.length !== 0;
    }
  }

  if (searchInput) {
    searchInput.addEventListener("input", function() {
      var queryText = searchInput.value.trim();
      var query = queryText.toLowerCase();
      if (!queryText) {
        clearSearchResults();
        return;
      }

      currentSearchQuery = queryText;

      var terms = query.split(/\s+/).filter(Boolean);
      var matches = searchIndex.filter(function(entry) {
        return terms.every(function(term) {
          return entry.text.indexOf(term) !== -1;
        });
      });

      if (!matches.some(function(entry) { return entry.id === activeSearchId; })) {
        activeSearchId = matches.length ? matches[0].id : null;
      }
      renderSearchResults(matches, queryText);
      renderSearchMinimap(matches, queryText);
    });
  }

  sidebarLinks.forEach(function(link) {
    link.addEventListener("click", function() {
      if (!currentSearchQuery) {
        return;
      }
      var id = (link.getAttribute("href") || "").replace(/^#/, "");
      if (!id || id === "header_wrap") {
        return;
      }
      var exists = currentSearchMatches.some(function(match) { return match.id === id; });
      if (!exists) {
        return;
      }
      activeSearchId = id;
      renderSearchResults(currentSearchMatches, currentSearchQuery);
      var target = document.getElementById(id);
      var highlightResult = highlightSearchTerms(target, currentSearchQuery);
      var blocks = highlightResult ? highlightResult.blocks : searchMinimapBlocksForMatch(id);
      setSearchMinimapHitBlocks(blocks);
      setSearchMinimapActiveBlock((highlightResult && highlightResult.first) || (blocks.length ? blocks[0] : null));
    });
  });

  window.addEventListener("scroll", updateSearchMinimapViewport, { passive: true });
  window.addEventListener("resize", updateSearchMinimapViewport);
  updateSearchMinimapViewport();

  if (!("IntersectionObserver" in window)) {
    return;
  }

  var activeLink = null;

  function setActive(link) {
    if (activeLink === link) {
      return;
    }
    if (activeLink) {
      activeLink.classList.remove("is-active");
    }
    activeLink = link;
    if (activeLink) {
      activeLink.classList.add("is-active");
    }
  }

  var observer = new IntersectionObserver(function(entries) {
    var visible = entries
      .filter(function(entry) { return entry.isIntersecting; })
      .sort(function(a, b) { return a.boundingClientRect.top - b.boundingClientRect.top; });

    if (visible.length) {
      setActive(linkMap.get(visible[0].target) || null);
    }
  }, {
    rootMargin: "-18% 0px -70% 0px",
    threshold: [0, 1]
  });

  linkMap.forEach(function(_, target) {
    observer.observe(target);
  });

  var fallback = document.getElementById("introduction");
  if (fallback && linkMap.has(fallback)) {
    setActive(linkMap.get(fallback));
  }
})();
