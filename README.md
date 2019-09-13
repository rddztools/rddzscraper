# RDDZ Scraper

## Description

RDDZ Scraper is a web scraper which runs on Linux, Windows & Mac (developed with [Qt](https://www.qt.io) framework).

It uses the power of XPath to be able to scrape anything even javascript
generated websites.
RDDZ Scraper can scrape standard targets like Google, Bing,
Yahoo... as well as not so standard ones like Ebay.com, Amazon.com,
... (and much much more)

### Several Tools
When your scrape is done (or when your import is done), you can do a lot of actions on your results :
- Trim to root
- Trim to last folder
- Trim to root domain
- Retrieve HTTP Status
- Resolve redirections
- Check percentage of dofollow links
- Get backlinks data (if you use one of the API below)
- Get number of outbound links
- Detect platform category and CMS used (if possible)
- Get IP address
- Link alive (check if backlink is present)
- Domain available
- Transfer all results to custom1 field (append or diff and append)
- Find and replace (can be a regular expression)
- Delete, with some cool filters
- Regex mask : remove pattern on each result
- Remove subdomains
- Remove duplicate domain or url
- Remove bad urls (need HTTP Status code check before)
- Remove selected URL
- Clear all results



### APIS
RDDZ Scraper integrate several SEO APIs too :
- MajesticSEO (Backlinks metrics)
- Ahrefs (Backlinks metrics)
- Moz (Backlinks metrics)
- SEObserver (Backlinks metrics)
- RapidAPI (domain availability)
- Dynadot (domain availability)

### Automation
RDDZ Scraper can also chain/automate tasks like :
- http status
- resolve redirection
- remove bad url
- dofollow %
- backlinks metrics (if one of previous API is used)
- outbound links
- get IP addresses

RDDZ Scaper can also be configured to use several list of keywords,
to take scraping even further.

### Proxies
Because web scraping isnt viewed as a "friendly" task, we ve added
a proxy manager tab, so it can scrap even when too fast or to agressive.

## Installation

### Needed dependencies
You need to download specific binary/files for your platform and place it into the `dist` directory :
* [xidel](http://www.videlibri.de/xidel.html#downloads)
* [phantomjs](https://phantomjs.org/download.html)
* [wappalyzer](https://github.com/AliasIO/Wappalyzer/blob/master/src/apps.json) Remove the line `  "$schema": "../schema.json",` !!

When everything is downloaded, your `dist` directory will contains these 3 files :
* xidel(.exe)
* phantomjs(.exe)
* apps.json

### System dependencies (Especially for windows platform)
* [openssl](https://www.openssl.org/source/)

### Compilation

```
git clone https://github.com/rddztools/rddzscraper.git
cd rddzscraper
qt-creator RDDZ_Scraper.pro
```

### API (backlinks) usage
If you use Majestic or SEObserver API, you need to sepcify a private key.
Just create a group in RDDZ_Scraper.ini and add value for each API service.
Like this :
```
...

[ExternalPrivateKeys]
Majestic=YOURPRIVATEKEY
SEObserver=YOURPRIVATEKEY

...
```


## Contact / Info
This project has been successfully built with QT 5.13.0 on Windows, Mac and Linux.

If you need help for anything XPath related, you can contact us through www.rddz.fr


