from __future__ import print_function
from pprint import pprint
import time
from .common import IsolatedTestCase
from relevanced_client import (
    EDocumentDoesNotExist,
    ECentroidDoesNotExist,
    EDocumentAlreadyInCentroid,
    EDocumentNotInCentroid
)

class TestBasicTextSimilarityScoring(IsolatedTestCase):
    def test_basic_text_similarity_1(self):
        self.client.create_centroid('centroid-1')
        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.create_document_with_id('doc-2', 'wolf wolf fish')
        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.add_document_to_centroid('centroid-1', 'doc-2')
        self.client.join_centroid('centroid-1')
        response = self.client.get_text_similarity('centroid-1', 'wolf wolf wolf')
        self.assertTrue(isinstance(response, float))
        self.assertTrue(response > 0.5)
        self.assertTrue(response < 1.0)

    def test_basic_text_similarity_2_zero_relevance(self):
        self.client.create_centroid('centroid-1')
        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.create_document_with_id('doc-2', 'wolf wolf fish')
        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.add_document_to_centroid('centroid-1', 'doc-2')
        self.client.join_centroid('centroid-1')
        response = self.client.get_text_similarity('centroid-1', 'apple orange banana')
        self.assertTrue(isinstance(response, float))
        self.assertTrue(response >= 0.0)
        self.assertTrue(response < 0.0001)

    def test_basic_text_similarity_just_one_document(self):
        # this checks for a previously fixed bug
        self.client.create_centroid('centroid-1')
        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.join_centroid('centroid-1')
        response = self.client.get_text_similarity('centroid-1', 'wolf wolf wolf')
        self.assertTrue(isinstance(response, float))
        self.assertTrue(response > 0.3)
        self.assertTrue(response < 1.0)

    def test_text_similarity_missing_centroid(self):
        with self.assertRaises(ECentroidDoesNotExist):
            self.client.get_text_similarity('centroid-1', 'some text')


class TestBasicMultiTextSimilarity(IsolatedTestCase):
    def test_multi_get_text_similarity_1(self):
        self.client.create_centroid('centroid-1')
        self.client.create_centroid('centroid-2')

        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.create_document_with_id('doc-2', 'wolf wolf fish')
        self.client.create_document_with_id('doc-3', 'fish banana fish')

        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.add_document_to_centroid('centroid-1', 'doc-2')

        self.client.add_document_to_centroid('centroid-2', 'doc-2')
        self.client.add_document_to_centroid('centroid-2', 'doc-3')

        response = self.client.multi_get_text_similarity(
            ['centroid-1', 'centroid-2'],
            'wolf banana'
        )
        self.assertEqual(
            set(['centroid-1', 'centroid-2']),
            set(response.scores.keys())
        )
        scores = response.scores
        self.assertTrue(scores['centroid-1'] < 1.0 and scores['centroid-1'] > 0.1)
        self.assertTrue(scores['centroid-2'] < 1.0 and scores['centroid-2'] > 0.1)


class TestBasicDocumentSimilarityScoring(IsolatedTestCase):
    def test_basic_document_similarity_1(self):
        self.client.create_centroid('centroid-1')
        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.create_document_with_id('doc-2', 'wolf wolf fish')
        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.add_document_to_centroid('centroid-1', 'doc-2')
        self.client.join_centroid('centroid-1')
        self.client.create_document_with_id('doc-to-test', 'wolf wolf wolf')
        response = self.client.get_document_similarity('centroid-1', 'doc-to-test')
        self.assertTrue(isinstance(response, float))
        self.assertTrue(response > 0.5)
        self.assertTrue(response < 1.0)

    def test_basic_document_similarity_2_zero_relevance(self):
        self.client.create_centroid('centroid-1')
        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.create_document_with_id('doc-2', 'wolf wolf fish')
        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.add_document_to_centroid('centroid-1', 'doc-2')
        self.client.join_centroid('centroid-1')
        self.client.create_document_with_id('doc-to-test', 'apple orange banana')
        response = self.client.get_document_similarity('centroid-1', 'doc-to-test')
        self.assertTrue(isinstance(response, float))
        self.assertTrue(response >= 0.0)
        self.assertTrue(response < 0.0001)

    def test_basic_document_similarity_just_one_document(self):
        # this checks for a previously fixed bug
        self.client.create_centroid('centroid-1')
        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.join_centroid('centroid-1')
        self.client.create_document_with_id('doc-to-test', 'wolf wolf wolf')
        response = self.client.get_document_similarity('centroid-1', 'doc-to-test')
        self.assertTrue(isinstance(response, float))
        self.assertTrue(response > 0.3)
        self.assertTrue(response < 1.0)

    def test_document_similarity_missing_centroid(self):
        self.client.create_document_with_id('doc-1', 'some text')
        with self.assertRaises(ECentroidDoesNotExist):
            self.client.get_document_similarity('some-centroid', 'doc-1')

    def test_document_similarity_missing_document(self):
        self.client.create_centroid('some-centroid')
        with self.assertRaises(EDocumentDoesNotExist):
            self.client.get_document_similarity('some-centroid', 'doc-1')

    def test_document_similarity_missing_centroid_and_document(self):
        with self.assertRaises(ECentroidDoesNotExist):
            self.client.get_document_similarity('some-centroid', 'some-doc')


class TestBasicCentroidSimilarityScoring(IsolatedTestCase):
    def test_centroid_similarity_scoring_1(self):
        self.client.create_centroid('centroid-1')
        self.client.create_centroid('centroid-2')
        self.client.create_document_with_id('doc-1', 'dog cat wolf wolf')
        self.client.create_document_with_id('doc-2', 'wolf wolf fish')
        self.client.create_document_with_id('doc-3', 'apple fish fish')

        self.client.add_document_to_centroid('centroid-1', 'doc-1')
        self.client.add_document_to_centroid('centroid-1', 'doc-2')

        self.client.add_document_to_centroid('centroid-2', 'doc-2')
        self.client.add_document_to_centroid('centroid-2', 'doc-3')

        self.client.join_centroid('centroid-1')
        self.client.join_centroid('centroid-2')

        response = self.client.get_centroid_similarity('centroid-1', 'centroid-2')
        self.assertTrue(isinstance(response, float))
        self.assertTrue(response > 0.1)
        self.assertTrue(response < 1.0)

    def test_get_centroid_similarity_missing_one_1(self):
        self.client.create_centroid('centroid-1')
        with self.assertRaises(ECentroidDoesNotExist):
            self.client.get_centroid_similarity('centroid-1', 'centroid-2')

    def test_get_centroid_similarity_missing_one_2(self):
        self.client.create_centroid('centroid-2')
        with self.assertRaises(ECentroidDoesNotExist):
            self.client.get_centroid_similarity('centroid-1', 'centroid-2')

    def test_get_centroid_similarity_missing_both(self):
        with self.assertRaises(ECentroidDoesNotExist):
            self.client.get_centroid_similarity('centroid-1', 'centroid-2')
